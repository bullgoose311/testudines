#ifdef _win64

#include "IOCPOutputStream.h"

#include "IOCPConnection.h"
#include "thread_utils.h"

IOCPOutputStream::IOCPOutputStream() : IOCPStream()
{
	InitializeCriticalSection(&m_lock);
}

void IOCPOutputStream::OnIocpCompletionPacket(DWORD bytesSent)
{
	if (!m_pConnection->IsConnected())
	{
		return;
	}

	if (bytesSent == 0)
	{
		LogError("unexpectedly sent 0 bytes");
		return;
	}

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
	if (!m_pConnection->IsConnected())
	{
		return false;
	}

	if (size > MAX_MESSAGE_SIZE)
	{
		LogError("Invalid message size");
		return false;
	}

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
	int errorCode = WSAGetLastError();
	if (result == SOCKET_ERROR && errorCode != ERROR_IO_PENDING)
	{
		LogError("WSASend failed", WSAGetLastError());
		m_pConnection->Reset();
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

#endif // #ifdef _win64