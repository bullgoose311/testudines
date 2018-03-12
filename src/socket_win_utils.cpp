#include "socket_win_utils.h"

#include <stdio.h>
#include <winerror.h>

void MapWsaErrorCodeToString(int errorCode, char* errorCodeString, size_t size)
{
	switch (errorCode)
	{
	case WSAEFAULT:
		sprintf_s(errorCodeString, size, "invalid overlapped pointer");
		break;
	case WSAEINVAL:
		sprintf_s(errorCodeString, size, "invalid parameter");
		break;
	case WSAENOTCONN:
		sprintf_s(errorCodeString, size, "socket not connected");
		break;
	case WSAENETDOWN:
		sprintf_s(errorCodeString, size, "network subsystem failed");
		break;
	case WSAEINPROGRESS:
		sprintf_s(errorCodeString, size, "blocking call in progress");
		break;
	case WSAENETRESET:
		sprintf_s(errorCodeString, size, "connection timed out");
		break;
	case WSAECONNABORTED:
		sprintf_s(errorCodeString, size, "connection aborted");
		break;
	default:
		sprintf_s(errorCodeString, size, "unknown error code: %d", errorCode);
		break;
	}
}