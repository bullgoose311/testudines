#pragma once

#include "common_defines.h"
#include "common_types.h"
#include "semaphore.h"

#include <winsock2.h>

#define MAX_SOCKET_BUFFER_SIZE 1024

class IOCPPacketHandler : public OVERLAPPED
{
public:
	IOCPPacketHandler();
	virtual void OnIocpCompletionPacket(DWORD bytesTransferred) = 0;
};

class IOCPStreamClosedListener
{
public:
	virtual void OnStreamClosed() = 0;
};

class IOCPStream : public IOCPPacketHandler
{
public:
	IOCPStream() : IOCPPacketHandler() {}

	virtual void Initialize(connectionId_t connectionId, IOCPStreamClosedListener* pListener);
	virtual void OnSocketAccept(SOCKET socket);
	virtual void OnIocpCompletionPacket(DWORD bytesTransferred) override = 0;

protected:
	connectionId_t		m_connectionId = 0;
	IOCPStreamClosedListener* m_pListener = nullptr;
	requestId_t			m_requestId = 0;
	SOCKET				m_socket = INVALID_SOCKET;

	char				m_messageBuffer[MAX_MESSAGE_SIZE] = { 0 };
	size_t				m_currentMessageSize = 0;
	bool				m_bAwaitingReset = false;

	void IssueReset();
	void ClearMessageBuffer();
	void LogInfo(const char* msg);
	void LogError(const char* msg);
	void LogError(const char*msg, int errorCode);
};

class IOCPInputStream : public IOCPStream
{
public:
	IOCPInputStream() : IOCPStream() {}

	virtual void OnSocketAccept(SOCKET socket) override;
	virtual void OnIocpCompletionPacket(DWORD bytesTransferred) override;

private:
	char m_socketBuffer[MAX_SOCKET_BUFFER_SIZE] = { 0 };

	void IssueRecv();
	void ClearSocketBuffer();
};

class IOCPOutputStream : public IOCPStream
{
public:
	IOCPOutputStream() : IOCPStream() {}

	// IOCPStream
	virtual void Initialize(connectionId_t connectionId, IOCPStreamClosedListener* pListener) override;
	virtual void OnIocpCompletionPacket(DWORD bytesTransferred) override;

	void Write(const char* msg, messageSize_t size);

private:
	char m_tempMessageBuffer[MAX_MESSAGE_SIZE];
	Semaphore m_sendSemaphore;

	void IssueSend();
};