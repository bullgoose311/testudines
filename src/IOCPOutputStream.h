#pragma once

#include "IOCPStream.h"

#define OUTGOING_MSG_BUFFER_SIZE 4096 * 10 // 40K per connection?

class IOCPOutputStream : public IOCPStream
{
public:
	IOCPOutputStream();

	// IOCPStream
	virtual void OnIocpCompletionPacket(DWORD bytesTransferred) override;

	bool Write(const char* msg, messageSize_t size);

protected:
	virtual void ClearBuffers();

private:
	char	m_outgoingMsgBuffer[OUTGOING_MSG_BUFFER_SIZE] = { 0 };
	size_t	m_outgoingMsgBufferSize = 0;
	bool m_bAwaitingIOCP = false;
	CRITICAL_SECTION m_lock;

	void IssueSend();
};