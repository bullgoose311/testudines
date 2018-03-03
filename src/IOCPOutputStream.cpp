#include "IOCPOutputStream.h"

#include "thread_utils.h"

IOCPOutputStream::IOCPOutputStream() : IOCPStream()
{
	InitializeCriticalSection(&m_lock);
}

void IOCPOutputStream::Initialize(connectionId_t connectionId, StreamClosedHandler* pStreamClosedHandler)
{
	IOCPStream::Initialize(connectionId, pStreamClosedHandler);
}

/*
- The message queue thread grabs a message off the queue, and calls send on that connection
- Before the IOCP signals that message is complete, the message queue thread pulls another message
for that message and attempts to send, the message queue thread is now blocked
- Meanwhile, the IOCP thread is trying to push a message onto the incoming queue but is blocked because it's full
meaning the IOCP thread is unable to unblock the outgoing message queue thread
- The http thread is also blocked trying push a message on to the ougoing message queue which is full
- DEADLOCK

- What if the outgoing message queue thread were to check and see if the outgoing stream were ready and if not
it just re-queued the message instead of blocking?
*/

void IOCPOutputStream::OnIocpCompletionPacket(DWORD bytesSent)
{
	if (bytesSent == 0)
	{
		// The client has closed the connection
		IssueReset();
		return;
	}

	/*
	** IOCP Thread
	- acquire the lock
	- if the send buffer is empty
	- bAwaitingBufferFlush = false
	- else
	- adjust the buffer based on the number of bytes sent and issue another send to IOCP
	*/

	{
		SCOPED_CRITICAL_SECTION(&m_lock);

		size_t bytesLeft = m_outgoingMsgBufferSize - bytesSent;

		if (bytesLeft == 0)
		{
			ZeroMemory(m_outgoingMsgBuffer, OUTGOING_MSG_BUFFER_SIZE);
			m_outgoingMsgBufferSize = 0;
			m_bAwaitingIOCP = false;
		}
		else
		{
			/*
			TODO: Instead of moving all the bytes down in the array, can we do something quicker like double buffer?
			*/
			for (int i = 0; i < bytesLeft; i++)
			{
				m_outgoingMsgBuffer[i] = m_outgoingMsgBuffer[bytesSent + i];
			}
			size_t bytesToZeroOut = OUTGOING_MSG_BUFFER_SIZE - bytesLeft;
			ZeroMemory(m_outgoingMsgBuffer + bytesLeft, bytesToZeroOut);
			m_outgoingMsgBufferSize = bytesLeft;
			IssueSend();
		}
	}
}

bool IOCPOutputStream::Write(const char* msg, messageSize_t size)
{
	if (size > MAX_MESSAGE_SIZE)
	{
		LogError("Invalid message size");
		return false;
	}

	/*
	** HTTP Thread
	- acquire the lock
	- Buffer the data and block if the buffer is full // Note buffer can't be bigger than IOCP buffer
	- if (bAwaitingBufferFlush)
	- return and let the IOCP thread handle it
	- else
	- issue send to IOCP
	- bAwaitingBufferFlush = true
	*/

	{
		SCOPED_CRITICAL_SECTION(&m_lock);

		// TODO: What to do if the buffer is full?  Block the HTTP thread until there's room?
		if ((m_outgoingMsgBufferSize + size) > OUTGOING_MSG_BUFFER_SIZE)
		{
			return false;
		}

		strncat_s(m_outgoingMsgBuffer, OUTGOING_MSG_BUFFER_SIZE, msg, size);
		m_outgoingMsgBufferSize += size;

		if (!m_bAwaitingIOCP)
		{
			IssueSend();
		}
	}

	return true;
}

void IOCPOutputStream::IssueSend()
{
	WSABUF wsabuf;
	wsabuf.buf = m_outgoingMsgBuffer;
	wsabuf.len = (DWORD)m_outgoingMsgBufferSize;
	DWORD bytesSent = 0;
	int result = WSASend(m_socket, &wsabuf, 1, &bytesSent, 0, this, nullptr);
	if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		LogError("WSASend failed", WSAGetLastError());
		IssueReset();
	}
	else
	{
		m_bAwaitingIOCP = true;
	}
}

void IOCPOutputStream::ClearBuffers()
{
	ZeroMemory(m_outgoingMsgBuffer, MAX_MESSAGE_SIZE);
	m_outgoingMsgBufferSize = 0;
}