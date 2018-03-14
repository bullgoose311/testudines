#include "IOCPInputStream.h"

#include "IOCPConnection.h"
#include "message_queue.h"

static const timeout_t QUEUE_TIMEOUT = 1000;
static const char MESSAGE_DELIMITER[] = "\r\n"; // This is delimiter used by our test app putty
static const int MESSAGE_DELIMITER_SIZE = 2;

extern MessageQueue g_incomingMessageQueue;

void IOCPInputStream::OnSocketAccept(SOCKET socket)
{
	IOCPStream::OnSocketAccept(socket);
	IssueRecv();
}

void IOCPInputStream::OnIocpCompletionPacket(DWORD bytesReceived)
{
	if (!m_pConnection->IsConnected())
	{
		return;
	}

	if (bytesReceived == 0)
	{
		LogInfo("connection cleanly closed by client");
		m_pConnection->Reset();
		return;
	}

	if (bytesReceived < 0)
	{
		LogError("connection unexpectedly closed");
		m_pConnection->Reset();
		return;
	}

	if ((m_messageBufferSize + bytesReceived) >= MAX_MESSAGE_SIZE)
	{
		LogError("we've reached the max message buffer size, issuing a disconnect");
		m_pConnection->Reset();
		return;
	}

	int i = 0;
	while (i < (int)bytesReceived)
	{
		bool eof = true;
		for (int j = 0; j < MESSAGE_DELIMITER_SIZE; j++)
		{
			if (m_socketBuffer[i + j] != MESSAGE_DELIMITER[j])
			{
				eof = false;
				break;
			}
		}

		if (eof)
		{
			// char msg[256];
			// sprintf_s(msg, "Connection %d: Received request %d", m_connectionId, m_requestId);
			// printf("INFO: %s\n", msg);
			g_incomingMessageQueue.enqueue(m_pConnection->GetConnectionId(), m_requestId, m_messageBuffer, m_messageBufferSize, QUEUE_TIMEOUT);
			ClearMessageBuffer();
			m_requestId++;
			i += MESSAGE_DELIMITER_SIZE;
		}
		else
		{
			m_messageBuffer[m_messageBufferSize] = m_socketBuffer[i];
			m_messageBufferSize++;
			i++;
		}
	}

	IssueRecv();
}

void IOCPInputStream::ClearBuffers()
{
	ClearMessageBuffer();
	ClearSocketBuffer();
}

void IOCPInputStream::ClearMessageBuffer()
{
	ZeroMemory(m_messageBuffer, MAX_MESSAGE_SIZE);
	m_messageBufferSize = 0;
}

void IOCPInputStream::IssueRecv()
{
	// LogInfo("ISSUE RECV");

	ClearSocketBuffer();

	WSABUF wsabuf;
	wsabuf.buf = m_socketBuffer;
	wsabuf.len = MAX_SOCKET_BUFFER_SIZE;
	DWORD flags = 0;
	DWORD bytesReceived = 0;
	int result = WSARecv(m_socket, &wsabuf, 1, &bytesReceived, &flags, (OVERLAPPED*)this, NULL);
	if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		LogError("WSARecv failed", WSAGetLastError());
		m_pConnection->Reset();
	}
}

void IOCPInputStream::ClearSocketBuffer()
{
	ZeroMemory(m_socketBuffer, MAX_SOCKET_BUFFER_SIZE);
}