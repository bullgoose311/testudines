#pragma once

#ifdef _win64

#include "IOCPPacketHandler.h"

#include "common_defines.h"
#include "common_types.h"

class IOCPConnection;

class IOCPStream : public IOCPPacketHandler
{
public:
	IOCPStream() : IOCPPacketHandler() {}

	virtual void Initialize(IOCPConnection *pConnection);
	virtual void OnSocketAccept(SOCKET socket);

	// IOCPPacketHandler
	virtual void OnIocpCompletionPacket(DWORD bytesTransferred) override = 0;
	virtual void OnIocpError() override;

protected:
	IOCPConnection*		m_pConnection = nullptr;
	requestId_t			m_requestId = 0;
	SOCKET				m_socket = INVALID_SOCKET;

	void LogInfo(const char* msg);
	void LogError(const char* msg);
	void LogError(const char*msg, int errorCode);

	virtual void ClearBuffers() = 0;
};

#endif // #ifdef _win64