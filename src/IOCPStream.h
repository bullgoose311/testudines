#pragma once

#include "IOCPPacketHandler.h"

class StreamClosedHandler
{
public:
	virtual void OnStreamClosed() = 0;
};

class IOCPStream : public IOCPPacketHandler
{
public:
	IOCPStream() : IOCPPacketHandler() {}

	virtual void Initialize(connectionId_t connectionId, StreamClosedHandler* pStreamClosedHandler);
	virtual void OnSocketAccept(SOCKET socket);
	virtual void OnIocpCompletionPacket(DWORD bytesTransferred) override = 0;

protected:
	connectionId_t		m_connectionId = 0;
	StreamClosedHandler* m_pStreamClosedHandler = nullptr;
	requestId_t			m_requestId = 0;
	SOCKET				m_socket = INVALID_SOCKET;

	bool				m_bAwaitingReset = false;

	void IssueReset();
	void LogInfo(const char* msg);
	void LogError(const char* msg);
	void LogError(const char*msg, int errorCode);

	virtual void ClearBuffers() = 0;
};