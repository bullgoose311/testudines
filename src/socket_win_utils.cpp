#include "socket_win_utils.h"

#include "log.h"

#include <stdio.h>
#include <winerror.h>

void MapWsaErrorCodeToString(int errorCode, char* errorCodeString, size_t size)
{
	switch (errorCode)
	{
	case WSAEFAULT:
		Log_PrintError(errorCodeString, size, "invalid overlapped pointer");
		break;
	case WSAEINVAL:
		Log_PrintError(errorCodeString, size, "invalid parameter");
		break;
	case WSAENOTCONN:
		Log_PrintError(errorCodeString, size, "socket not connected");
		break;
	case WSAENETDOWN:
		Log_PrintError(errorCodeString, size, "network subsystem failed");
		break;
	case WSAEINPROGRESS:
		Log_PrintError(errorCodeString, size, "blocking call in progress");
		break;
	case WSAENETRESET:
		Log_PrintError(errorCodeString, size, "connection timed out");
		break;
	case WSAECONNABORTED:
		Log_PrintError(errorCodeString, size, "connection aborted");
		break;
	default:
		Log_PrintError(errorCodeString, size, "unknown error code: %d", errorCode);
		break;
	}
}