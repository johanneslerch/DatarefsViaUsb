#include "DatarefsViaUsb.h"


#ifndef XPLM300
	#error This is made to be compiled against the XPLM300 SDK
#endif


std::list<connection_t> connections;

static void Send(HANDLE handle, uint8 data[], uint8 dataLength) {
	DWORD dNoOfBytesWritten = 0;
	WriteFile(handle, data, dataLength, &dNoOfBytesWritten, NULL);
}

static void SendInit(HANDLE handle) {
	uint8 data[1] = { INIT };
	Send(handle, data, 1u);
}

static void SendDatarefUpdate(HANDLE handle, uint8 datarefIndex, dataref_t *dataref) {
	uint8 data[BUFFER_SIZE];
	data[0] = UPDATE;
	data[1] = datarefIndex;
	memcpy(&data[2], &(dataref->value), sizeof(dataref->value));
	uint8 dataLength = 2 + sizeof(dataref->value);
	Send(handle, data, dataLength);
}

void Connect(LPCSTR portName) {
	HANDLE hComm = CreateFile(portName,                //port name
		GENERIC_READ | GENERIC_WRITE,                //Read/Write
		0,                // No Sharing
		NULL,                // No Security
		OPEN_EXISTING,                // Open existing port only
		0,                // Non Overlapped I/O
		NULL);                // Null for Comm Devices

	if (hComm == INVALID_HANDLE_VALUE)
		printf("Error in opening serial port\n");
	else {
		printf("opening serial port successful\n");


		DCB dcbSerialParams = { 0 }; // Initializing DCB structure
		dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
		BOOL Status = GetCommState(hComm, &dcbSerialParams);

		dcbSerialParams.BaudRate = CBR_128000;  // Setting BaudRate
		dcbSerialParams.ByteSize = 8;         // Setting ByteSize = 8
		dcbSerialParams.StopBits = ONESTOPBIT;// Setting StopBits = 1
		dcbSerialParams.Parity = NOPARITY;  // Setting Parity = None
		SetCommState(hComm, &dcbSerialParams);

		COMMTIMEOUTS timeouts = { 0, //interval timeout. 0 = not used
								  0, // read multiplier
								  1, // read constant (milliseconds)
								  0, // Write multiplier
								  0  // Write Constant
		};
		SetCommTimeouts(hComm, &timeouts);

		connection_t conn = { portName, hComm, {} };
		connections.push_back(conn);
		SendInit(conn.handle);
	}
}

static void Disconnect() {
	std::list<connection_t>::iterator conn = connections.begin();
	while (conn != connections.end()) {
		CloseHandle(conn->handle);
		conn++;
	}
	connections.clear();
}

static datarefValue_t ReadDatarefValue(dataref_t *dataref) {
	switch (dataref->type) {
	case INT_4BYTES:
		return XPLMGetDatai(dataref->handle);
		break;
	case FLOAT_4BYTES:
		float value = XPLMGetDataf(dataref->handle);
		datarefValue_t result = 0;
		memcpy((uint8*)&result, (uint8*)&value, sizeof(datarefValue_t));
		return result;
		break;
	}
}

static void WriteDatarefValue(XPLMDataRef handle, datatype_t type, datarefValue_t value) {
	switch (type) {
	case INT_4BYTES:
		XPLMSetDatai(handle, value);
		break;
	case FLOAT_4BYTES:
		float floatValue = 0.0f;
		memcpy((uint8*)&floatValue, (uint8*)&value, sizeof(datarefValue_t));
		XPLMSetDataf(handle, floatValue);
		break;
	}
}

static void UpdateDatarefs(connection_t *conn) {
	std::list<dataref_t>::iterator dataref = conn->datarefs.begin();
	uint8 datarefIndex = 0;
	while (dataref != conn->datarefs.end()) {
		bool updateRequired = false;
		if (dataref->handle == NULL) {
			dataref->handle = XPLMFindDataRef(dataref->label);
			updateRequired = true;
		}
		if (dataref->handle != NULL) {
			datarefValue_t value = ReadDatarefValue(&(*dataref));
			if (value != dataref->value) {
				updateRequired = true;
			}
			if (updateRequired) {
				dataref->value = value;
				SendDatarefUpdate(conn->handle, datarefIndex, &(*dataref));
			}
		}

		dataref++;
		datarefIndex++;
	}
}

static void HandleInput(connection_t *conn) {
	uint8 readBuffer[BUFFER_SIZE] = { 0 };
	DWORD bytesRead = 0;
	ReadFile(conn->handle, &readBuffer, BUFFER_SIZE, &bytesRead, NULL);
	if (bytesRead > 0) {
		switch ((messageFromClient_t)readBuffer[0]) {
		case SUBSCRIBE: 
			{
				dataref_t dataref = {};
				dataref.type = (datatype_t)readBuffer[1];
				strncpy(dataref.label, (const char*)&readBuffer[2], min(bytesRead - 2, MAX_DATAREF_LABEL_LENGTH));
				dataref.handle = XPLMFindDataRef(dataref.label);
				if (dataref.handle != NULL) {
					dataref.value = ReadDatarefValue(&dataref);
					conn->datarefs.push_back(dataref);
					SendDatarefUpdate(conn->handle, conn->datarefs.size() - 1, &dataref);
				}
				break;
			}
		case COMMAND: 
			{
				XPLMCommandRef cmdHandle = XPLMFindCommand((const char*)&readBuffer[1]);
				if (cmdHandle != NULL)
					XPLMCommandOnce(cmdHandle);
				break;
			}
		case SET_DATAREF: 
			{
				datatype_t type = (datatype_t)readBuffer[1];
				datarefValue_t value = 0;
				memcpy((uint8*)&value, &readBuffer[2], sizeof(datarefValue_t));
				XPLMDataRef datarefHandle = XPLMFindDataRef((const char*)&readBuffer[1 + sizeof(datarefValue_t)]);
				if (datarefHandle != NULL) {
					WriteDatarefValue(datarefHandle, type, value);
				}
				break;
			}
		}
	}
}

PLUGIN_API float MyFlightLoopCallback(
	float inElapsedSinceLastCall,
	float inElapsedTimeSinceLastFlightLoop,
	int inCounter,
	void *inRefcon) {

	std::list<connection_t>::iterator conn = connections.begin();
	while (conn != connections.end()) {
		UpdateDatarefs(&(*conn));	
		HandleInput(&(*conn));
		conn++;
	}

	return 0.01f;
}

PLUGIN_API int XPluginStart(
							char *		outName,
							char *		outSig,
							char *		outDesc)
{
	strcpy(outName, "DatarefsViaUsb");
	strcpy(outSig, "DatarefsViaUsb");
	strcpy(outDesc, "Plugin to make Datarefs available via an USB Connection");
	return true;
}

PLUGIN_API void	XPluginStop(void)
{
	
}

PLUGIN_API void XPluginDisable(void) { 
	XPLMUnregisterFlightLoopCallback(MyFlightLoopCallback, NULL);
	Disconnect();
}

PLUGIN_API int XPluginEnable(void)  { 
	Connect("\\\\.\\COM4"); //TODO: Allow to define connections via GUI
	XPLMRegisterFlightLoopCallback(MyFlightLoopCallback, 0.01f, NULL);
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam) { }
