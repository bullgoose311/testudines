#ifdef _win64

#include "IOCPConnection.h"

#include "common_defines.h"
#include "log.h"
#include "socket_win_utils.h"

#include <mswsock.h> // AcceptEx

#pragma comment(lib,"mswsock")

extern bool g_verboseLogging;

IOCPConnection::~IOCPConnection()
{
	shutdown(m_socket, SD_BOTH);
	closesocket(m_socket);
}

bool IOCPConnection::Initialize(connectionId_t connectionId, SOCKET listenSocket, HANDLE hIOCP)
{
	if (listenSocket == INVALID_SOCKET || hIOCP == INVALID_HANDLE_VALUE)
	{
		LogError("Invalid listen socket or IOCP handle");
		return false;
	}

	// Overlapped
	Internal = 0;
	InternalHigh = 0;
	Offset = 0;
	OffsetHigh = 0;
	hEvent = NULL;

	m_connectionId = connectionId;
	m_listenSocket = listenSocket;
	m_hIOCP = hIOCP;

	m_inputStream.Initialize(this);
	m_outputStream.Initialize(this);

	InitializeSocket();

	HANDLE iocp = CreateIoCompletionPort((HANDLE)m_socket, m_hIOCP, COMPLETION_KEY_IO, 0);
	if (iocp != m_hIOCP)
	{
		char msg[32];
		sprintf_s(msg, "Failed to add connection %d to IOCP", m_connectionId);
		LogError(msg);
		return false;
	}

	IssueAccept();

	return true;
}

void IOCPConnection::InitializeSocket()
{
	/* People familiar with the standard accept API may be confused by the fact that a client socket is created prior to the call to AcceptEx,
	so let me explain. AcceptEx requires that the client socket be created up-front, but this minor annoyance has a payoff in the end:
	It lets a socket descriptor be reused for a new connection via a special call to DisconnectEx. This means that a server that deals
	with many short-lived connections can utilize a pool of allocated sockets without incurring the cost of creating new descriptors all
	the time. */
	m_socket = WSASocketW(PF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (m_socket == SOCKET_ERROR)
	{
		LogError("failed to initialize socket", WSAGetLastError());
	}
}

void IOCPConnection::OnIocpCompletionPacket(DWORD bytesTransferred)
{
	switch (m_state)
	{
	case IOCPConnectionState_e::AWAITING_ACCEPT:
		CompleteAccept();
		break;
	case IOCPConnectionState_e::AWAITING_DISCONNECT:
		CompleteDisconnect();
		break;
	default:
		LogError("connection received an IOCP packet while connected");
		break;
	}
}

void IOCPConnection::OnIocpError()
{
	// TODO: Should we Reset() or Disconnect() here?
	Reset();
}

void IOCPConnection::Reset()
{
	IssueDisconnect();
}

void IOCPConnection::IssueAccept()
{
	LogInfo("ISSUE ACCEPT");

	ZeroMemory(&m_addrBlock, ACCEPT_ADDR_LENGTH * 2);

	DWORD bytesReceived = 0;
	m_state = IOCPConnectionState_e::AWAITING_ACCEPT;
	bool succeeded = AcceptEx(m_listenSocket, m_socket, m_addrBlock, 0, ACCEPT_ADDR_LENGTH, ACCEPT_ADDR_LENGTH, &bytesReceived, this);
	if (!succeeded && WSAGetLastError() != ERROR_IO_PENDING)
	{
		// TODO: This connection is in a bad state, what do we do?
		LogError("AcceptEx failed", WSAGetLastError());
	}
}

void IOCPConnection::CompleteAccept()
{
	LogInfo("COMPLETE ACCEPT");

	/* When the AcceptEx function returns, the socket sAcceptSocket is in the default state for a connected socket.  The socket sAcceptSocket
	does not inherit the properties of the socket associated with sListenSocket parameter until SO_UPDATE_ACCEPT_CONTEXT is set on the
	socket.Use the setsockopt function to set the SO_UPDATE_ACCEPT_CONTEXT option, specifying sAcceptSocket as the socket handle and
	m_listenSocket as the option value.*/
	int result = setsockopt(m_socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&m_listenSocket, sizeof(SOCKET));
	if (result == SOCKET_ERROR)
	{
		// TODO: What do we do when this fails?
		LogError("setsockopt failed", WSAGetLastError());
		return;
	}

	m_state = IOCPConnectionState_e::CONNECTED;

	m_inputStream.OnSocketAccept(m_socket);
	m_outputStream.OnSocketAccept(m_socket);
}

void IOCPConnection::IssueDisconnect()
{
	LPFN_DISCONNECTEX pDisconnectEx = nullptr;
	GUID disconnectExGuid = WSAID_DISCONNECTEX;
	DWORD bytes = 0;
	int result = WSAIoctl(m_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &disconnectExGuid, sizeof(disconnectExGuid), &pDisconnectEx, sizeof(pDisconnectEx), &bytes, NULL, NULL);
	if (result != 0)
	{
		// TODO: This connection is in a bad state, what do we do?
		LogError("failed to load DisconnectEx extension, this connection is now useless", result);
		return;
	}

	m_state = IOCPConnectionState_e::AWAITING_DISCONNECT;
	bool succeeded = pDisconnectEx(m_socket, this, TF_REUSE_SOCKET, 0);
	if (!succeeded && WSAGetLastError() != ERROR_IO_PENDING)
	{
		// TODO: What to do when this fails?
		LogError("DisconnectEX failed", WSAGetLastError());
	}
}

void IOCPConnection::CompleteDisconnect()
{
	IssueAccept();
}

void IOCPConnection::LogError(const char* msg)
{
	Log_PrintError("Connection %d - Thread %d - %s", m_connectionId, GetCurrentThreadId(), msg);
}

void IOCPConnection::LogError(const char* msg, int errorCode)
{
	char errorCodeString[64];
	MapWsaErrorCodeToString(errorCode, errorCodeString, ARRAYSIZE(errorCodeString));
	Log_PrintError("%s: %s", msg, errorCodeString);
}

void IOCPConnection::LogInfo(const char* msg)
{
	if (g_verboseLogging)
	{
		Log_PrintInfo("Connection %d - Thread %d - %s", m_connectionId, GetCurrentThreadId(), msg);
	}
}

#endif // #ifdef _win64