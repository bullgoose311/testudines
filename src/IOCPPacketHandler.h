#pragma once

#include <winsock2.h>

#define MAX_SOCKET_BUFFER_SIZE 1024

class IOCPPacketHandler : public OVERLAPPED
{
public:
	IOCPPacketHandler();

	virtual void OnIocpCompletionPacket(DWORD bytesTransferred) = 0;
	virtual void OnIocpError() = 0;
};