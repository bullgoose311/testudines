#ifdef _win64

#include "IOCPPacketHandler.h"

IOCPPacketHandler::IOCPPacketHandler()
{
	// Overlapped
	Internal = 0;
	InternalHigh = 0;
	Offset = 0;
	OffsetHigh = 0;
	hEvent = NULL;
}

#endif // #ifdef _win64