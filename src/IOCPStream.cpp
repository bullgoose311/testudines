#include "IOCPStream.h"

#include "IOCPConnection.h"
#include "socket_win_utils.h"

#include <stdio.h> // sprintf

extern bool g_verboseLogging;

void IOCPStream::Initialize(IOCPConnection* pConnection)
{
	m_pConnection = pConnection;
}

void IOCPStream::OnSocketAccept(SOCKET socket)
{
	m_socket = socket;
	m_requestId = 0;
	ClearBuffers();
}

void IOCPStream::OnIocpError()
{
	m_pConnection->Reset();
}

void IOCPStream::LogInfo(const char* msg)
{
	if (g_verboseLogging)
	{
		printf("INFO: Connection %d - Thread %d - %s\n", m_pConnection->GetConnectionId(), GetCurrentThreadId(), msg);
	}
}

void IOCPStream::LogError(const char* msg)
{
	printf("ERROR: Connection %d - Thread %d - %s\n", m_pConnection->GetConnectionId(), GetCurrentThreadId(), msg);
}

void IOCPStream::LogError(const char* msg, int errorCode)
{
	char errorCodeString[64];
	MapWsaErrorCodeToString(errorCode, errorCodeString, ARRAYSIZE(errorCodeString));
	char formattedMsg[256];
	sprintf_s(formattedMsg, "%s: %s", msg, errorCodeString);
	LogError(formattedMsg);
}