#pragma once

#ifdef _win64

#include "IOCPStream.h"

class IOCPInputStream : public IOCPStream
{
public:
	IOCPInputStream() : IOCPStream() {}

	virtual void OnSocketAccept(SOCKET socket) override;
	virtual void OnIocpCompletionPacket(DWORD bytesTransferred) override;

protected:
	virtual void ClearBuffers();

private:
	char	m_socketBuffer[MAX_SOCKET_BUFFER_SIZE] = { 0 };
	char	m_messageBuffer[MAX_MESSAGE_SIZE] = { 0 };
	size_t	m_messageBufferSize = 0;

	void ClearMessageBuffer();
	void IssueRecv();
	void ClearSocketBuffer();
};

#endif // #ifdef _win64