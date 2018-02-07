#include "IOCPPacketHandler.h"

#include "message_queue.h"

#include <stdio.h> // sprintf
#include <mswsock.h> // AcceptEx

extern MessageQueue g_incomingMessageQueue;

static const timeout_t QUEUE_TIMEOUT = 1000;
static const char MESSAGE_DELIMITER[] = "\r\n"; // This is delimiter used by our test app putty
static const int MESSAGE_DELIMITER_SIZE = 2;

IOCPPacketHandler::IOCPPacketHandler()
{
	// Overlapped
	Internal = 0;
	InternalHigh = 0;
	Offset = 0;
	OffsetHigh = 0;
	hEvent = NULL;
}

// ***********************************************************
// IOCPStream
// ***********************************************************

void IOCPStream::Initialize(connectionId_t connectionId, IOCPStreamClosedListener* pListener)
{
	m_connectionId = connectionId;
	m_pListener = pListener;
}

void IOCPStream::OnSocketAccept(SOCKET socket)
{
	m_socket = socket;
	m_requestId = 0;
	ClearMessageBuffer();
}

void IOCPStream::IssueReset()
{
	m_pListener->OnStreamClosed();
}

void IOCPStream::ClearMessageBuffer()
{
	ZeroMemory(m_messageBuffer, MAX_MESSAGE_SIZE);
	m_currentMessageSize = 0;
}

void IOCPStream::LogInfo(const char* msg)
{
	printf("INFO: Connection %d - Thread Id - %d - %s\n", m_connectionId, GetCurrentThreadId(), msg);
}

void IOCPStream::LogError(const char* msg)
{
	printf("ERROR: Connection %d - Thread Id - %d - %s\n", m_connectionId, GetCurrentThreadId(), msg);
}

void IOCPStream::LogError(const char* msg, int errorCode)
{
	char formattedMsg[256];
	sprintf_s(formattedMsg, "%s: Error code %d", msg, errorCode);
	LogError(formattedMsg);
}

// ***********************************************************
// IOCPInputStream
// ***********************************************************

void IOCPInputStream::OnSocketAccept(SOCKET socket)
{
	IOCPStream::OnSocketAccept(socket);
	IssueRecv();
}

void IOCPInputStream::OnIocpCompletionPacket(DWORD bytesReceived)
{
	char msg[1024];
	sprintf_s(msg, "COMPLETE RECV (%d bytes)", (int)bytesReceived);
	LogInfo(msg);

	if (bytesReceived == 0)
	{
		// The client has closed the connection
		IssueReset();
		return;
	}

	if ((m_currentMessageSize + bytesReceived) >= MAX_MESSAGE_SIZE)
	{
		LogError("we've reached the max message buffer size, resetting connection");
		IssueReset();
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
			g_incomingMessageQueue.enqueue(m_connectionId, m_requestId, m_messageBuffer, m_currentMessageSize, QUEUE_TIMEOUT);
			ClearMessageBuffer();
			m_requestId++;
			i += MESSAGE_DELIMITER_SIZE;
		}
		else
		{
			m_messageBuffer[m_currentMessageSize] = m_socketBuffer[i];
			m_currentMessageSize++;
			i++;
		}
	}

	IssueRecv();
}

void IOCPInputStream::IssueRecv()
{
	LogInfo("ISSUE RECV");

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
		IssueReset();
	}
}

void IOCPInputStream::ClearSocketBuffer()
{
	ZeroMemory(m_socketBuffer, MAX_SOCKET_BUFFER_SIZE);
}

// ***********************************************************
// IOCPOutputStream
// ***********************************************************

void IOCPOutputStream::Initialize(connectionId_t connectionId, IOCPStreamClosedListener* pListener)
{
	IOCPStream::Initialize(connectionId, pListener);
	m_sendSemaphore.Signal();
}

void IOCPOutputStream::OnIocpCompletionPacket(DWORD bytesSent)
{
	char msg[1024];
	sprintf_s(msg, "COMPLETE SEND (%d bytes)", (int)bytesSent);
	LogInfo(msg);

	if (bytesSent == 0)
	{
		// The client has closed the connection
		LogInfo("SIGNALING CLOSE");
		m_sendSemaphore.Signal();
		IssueReset();
		return;
	}

	if (bytesSent != m_currentMessageSize)
	{
		const size_t bytesLeft = m_currentMessageSize - bytesSent;
		ZeroMemory(m_tempMessageBuffer, MAX_MESSAGE_SIZE);
		strncpy_s(m_tempMessageBuffer, MAX_MESSAGE_SIZE, m_messageBuffer + bytesSent, bytesLeft);
		ZeroMemory(m_messageBuffer, MAX_MESSAGE_SIZE);
		strncpy_s(m_messageBuffer, MAX_MESSAGE_SIZE, m_tempMessageBuffer, bytesLeft);
		m_currentMessageSize = bytesLeft;
		IssueSend();
	}
	else
	{
		LogInfo("SIGNALING DONE");
		m_sendSemaphore.Signal();
	}
}

void IOCPOutputStream::Write(const char* msg, messageSize_t size)
{
	LogInfo("WAIT WRITE");
	m_sendSemaphore.Wait();

	if (size > MAX_MESSAGE_SIZE)
	{
		LogError("Invalid message size");
		return;
	}

	ClearMessageBuffer();

	strncpy_s(m_messageBuffer, MAX_MESSAGE_SIZE, msg, size);
	m_currentMessageSize = size;

	IssueSend();
}

void IOCPOutputStream::IssueSend()
{
	LogInfo("ISSUE SEND");

	WSABUF wsabuf;
	wsabuf.buf = m_messageBuffer;
	wsabuf.len = (DWORD)m_currentMessageSize;
	DWORD bytesSent = 0;
	int result = WSASend(m_socket, &wsabuf, 1, &bytesSent, 0, this, nullptr);
	if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		LogError("WSASend failed", WSAGetLastError());
		IssueReset();
	}
}