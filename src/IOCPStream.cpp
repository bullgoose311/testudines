#include "IOCPStream.h"

#include <stdio.h> // sprintf

static const bool VERBOSE_LOGGING = true;

void IOCPStream::Initialize(connectionId_t connectionId, StreamClosedHandler* pStreamClosedHandler)
{
	m_connectionId = connectionId;
	m_pStreamClosedHandler = pStreamClosedHandler;
}

void IOCPStream::OnSocketAccept(SOCKET socket)
{
	m_socket = socket;
	m_requestId = 0;
	ClearBuffers();
}

void IOCPStream::IssueReset()
{
	m_pStreamClosedHandler->OnStreamClosed();
}

void IOCPStream::LogInfo(const char* msg)
{
	if (VERBOSE_LOGGING)
	{
		printf("INFO: Connection %d - Thread Id - %d - %s\n", m_connectionId, GetCurrentThreadId(), msg);
	}
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