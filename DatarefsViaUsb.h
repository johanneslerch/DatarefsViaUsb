#include <list>
#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
#include "XPLMUtilities.h"
#include "windows.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

typedef signed char sint8;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef signed short sint16;
typedef signed int sint32;

typedef sint32 datarefValue_t;

#define MAX_DATAREF_LABEL_LENGTH 128u
#define BUFFER_SIZE 128u

typedef enum {
	INT_4BYTES,
	FLOAT_4BYTES
} datatype_t;

typedef struct {
	char label[MAX_DATAREF_LABEL_LENGTH];
	datatype_t type;
	XPLMDataRef handle;
	datarefValue_t value;
} dataref_t;

typedef struct {
	LPCSTR portName;
	HANDLE handle;
	std::list<dataref_t> datarefs;
	uint8 buffer[BUFFER_SIZE];
	uint8 buffer_usage;
	bool initialized;
} connection_t;

typedef enum {
	INIT, //no additional information in message
	UPDATE // messages must be followed by 1 byte representing the index of the dataref (order of subscriptions), then sizeof(datarefValue_t) bytes representing the value of the dataref
} messageToClient_t;

typedef enum {
	SUBSCRIBE, //messages must be followed by 1 byte representing the datatype_t, then a string representing the name of the dataref
	COMMAND, //messages must be followed by string representing the name of the command
	SET_DATAREF, //messages must be followed by 1 byte representing the datatype_t, sizeof(datarefValue_t) bytes representing the new value, then a string representing the name of the dataref
	BEGIN_COMMAND, //messages must be followed by string representing the name of the command
	END_COMMAND, //messages must be followed by string representing the name of the command
} messageFromClient_t;
