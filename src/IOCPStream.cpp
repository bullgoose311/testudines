#ifdef _win64

#include "IOCPStream.h"

#include "IOCPConnection.h"
#include "log.h"
#include "socket_win_utils.h"

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
		Log_PrintInfo("Connection %d - Thread %d - %s", m_pConnection->GetConnectionId(), GetCurrentThreadId(), msg);
	}
}

void IOCPStream::LogError(const char* msg)
{
	Log_PrintError("Connection %d - Thread %d - %s", m_pConnection->GetConnectionId(), GetCurrentThreadId(), msg);
}

void IOCPStream::LogError(const char* msg, int errorCode)
{
	char errorCodeString[64];
	MapWsaErrorCodeToString(errorCode, errorCodeString, ARRAYSIZE(errorCodeString));
	Log_PrintError("%s: %s", msg, errorCodeString);
}

#endif // #ifdef _win64