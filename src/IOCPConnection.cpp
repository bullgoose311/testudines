#include "IOCPConnection.h"

#include <mswsock.h> // AcceptEx
#include <stdio.h> // sprintf

#pragma comment(lib,"mswsock")

IOCPConnection::~IOCPConnection()
{
	char msg[256];
	sprintf_s(msg, "shutting down connection %d", m_connectionId);
	LogInfo(msg);

	shutdown(m_socket, SD_BOTH);
	closesocket(m_socket);
}

bool IOCPConnection::Initialize(ConnectionId_t connectionId, SOCKET listenSocket, HANDLE hIOCP)
{
	char msg[256];
	sprintf_s(msg, "Initializing connection %d", connectionId);
	LogInfo(msg);

	if (listenSocket == INVALID_SOCKET || hIOCP == INVALID_HANDLE_VALUE)
	{
		LogError("Invalid listen socket or IOCP handle");
		return false;
	}

	m_connectionId = connectionId;
	m_listenSocket = listenSocket;
	m_hIOCP = hIOCP;

	// OVERLAPPED properties
	Internal = 0;
	InternalHigh = 0;
	Offset = 0;
	OffsetHigh = 0;
	hEvent = NULL;

	ClearBuffers();

	/* People familiar with the standard accept API may be confused by the fact that a client socket is created prior to the call to AcceptEx,
	so let me explain. AcceptEx requires that the client socket be created up-front, but this minor annoyance has a payoff in the end:
	It lets a socket descriptor be reused for a new connection via a special call to TransmitFile. This means that a server that deals
	with many short-lived connections can utilize a pool of allocated sockets without incurring the cost of creating new descriptors all
	the time. */
	m_socket = WSASocketW(PF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);

	HANDLE iocp = CreateIoCompletionPort((HANDLE)m_socket, m_hIOCP, COMPLETION_KEY_IO, 0);
	if (iocp != m_hIOCP)
	{
		char msg[256];
		sprintf_s(msg, "Failed to add connection %d to IOCP", m_connectionId);
		LogError(msg);
		return false;
	}

	IssueAccept();

	return true;
}

void IOCPConnection::OnIocpCompletionPacket(DWORD bytesTransferred)
{
	switch (m_state)
	{
	case ConnectionState_e::WAIT_ACCEPT:
		CompleteAccept();
		break;

	case ConnectionState_e::WAIT_RECV:
		CompleteRecv(bytesTransferred);
		break;

	case ConnectionState_e::WAIT_SEND:
		CompleteSend();
		break;

	case ConnectionState_e::WAIT_RESET:
		CompleteReset();
		break;
	}
}

void IOCPConnection::IssueAccept()
{
	LogInfo("ISSUE ACCEPT");

	m_state = ConnectionState_e::WAIT_ACCEPT;
	DWORD bytesReceived = 0;
	bool succeeded = AcceptEx(m_listenSocket, m_socket, m_addrBlock, 0, ACCEPT_ADDR_LENGTH, ACCEPT_ADDR_LENGTH, &bytesReceived, this);
	if (!succeeded && WSAGetLastError() != ERROR_IO_PENDING)
	{
		// TODO: Error handling
		LogError("AcceptEx failed", WSAGetLastError());
	}
}

void IOCPConnection::CompleteAccept()
{
	LogInfo("COMPLETE ACCEPT");

	/* When the AcceptEx function returns, the socket sAcceptSocket is in the default state for a connected socket.  The socket sAcceptSocket
	does not inherit the properties of the socket associated with sListenSocket parameter until SO_UPDATE_ACCEPT_CONTEXT is set on the
	socket.Use the setsockopt function to set the SO_UPDATE_ACCEPT_CONTEXT option, specifying sAcceptSocket as the socket handle and
	sListenSocket as the option value.*/
	int result = setsockopt(m_socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&m_listenSocket, sizeof(SOCKET));
	if (result == SOCKET_ERROR)
	{
		// TODO: Error handling
		LogError("setsockopt failed");
		return;
	}

	IssueRecv();
}

void IOCPConnection::IssueRecv()
{
	LogInfo("ISSUE RECV");

	m_state = ConnectionState_e::WAIT_RECV;
	WSABUF wsabuf;
	wsabuf.buf = m_socketBuffer;
	wsabuf.len = MAX_SOCKET_BUFFER_SIZE;
	DWORD flags = 0;
	DWORD bytesReceived = 0;
	int result = WSARecv(m_socket, &wsabuf, 1, &bytesReceived, &flags, (OVERLAPPED*)this, NULL);
	if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		// TODO: Error handling
		LogError("WSARecv failed", WSAGetLastError());
	}
}

void IOCPConnection::CompleteRecv(size_t bytesReceived)
{
	char msg[1024];
	sprintf_s(msg, "COMPLETE RECV (%d bytes): %s", (int)bytesReceived, m_socketBuffer);
	LogInfo(msg);

	// The client has closed the connection
	if (bytesReceived == 0)
	{
		IssueReset();
		return;
	}

	// TODO: How to handle multiple messages?  Here we'd need to queue the message as we parse it

	if ((m_currentMessageSize + bytesReceived) >= MAX_MESSAGE_BUFFER_SIZE)
	{
		LogError("we've reached the max message buffer size, resetting connection");
		IssueReset();
		return;
	}
	
	// TODO: Properly handle null termination here
	memcpy(m_messageBuffer + m_currentMessageSize, m_socketBuffer, bytesReceived);
	m_currentMessageSize += bytesReceived;

	if (m_messageBuffer[m_currentMessageSize - 1] == '\n')
	{
		IssueSend();
	}
	else
	{
		ZeroMemory(m_socketBuffer, MAX_SOCKET_BUFFER_SIZE);
		IssueRecv();
	}
}

void IOCPConnection::IssueSend()
{
	LogInfo("ISSUE SEND");

	m_state = ConnectionState_e::WAIT_SEND;
	WSABUF wsabuf;
	wsabuf.buf = m_messageBuffer;
	wsabuf.len = (DWORD)m_currentMessageSize;
	DWORD bytesSent = 0;
	int result = WSASend(m_socket, &wsabuf, 1, &bytesSent, 0, this, nullptr);
	if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		// TODO: Error handling
		LogError("WSASend failed", WSAGetLastError());
	}
}

void IOCPConnection::CompleteSend()
{
	LogInfo("COMPLETE SEND");

	ZeroMemory(m_socketBuffer, MAX_SOCKET_BUFFER_SIZE);
	ZeroMemory(m_messageBuffer, MAX_MESSAGE_BUFFER_SIZE);

	IssueRecv();
}

void IOCPConnection::IssueReset()
{
	LogInfo("ISSUE RESET");

	LPFN_DISCONNECTEX pDisconnectEx = nullptr;
	GUID disconnectExGuid = WSAID_DISCONNECTEX;
	DWORD bytes = 0;
	int result = WSAIoctl(m_socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &disconnectExGuid, sizeof(disconnectExGuid), &pDisconnectEx, sizeof(pDisconnectEx), &bytes, NULL, NULL);
	if (result != 0)
	{
		// TODO: Error handling
		LogError("failed to load DisconnectEx extension", result);
		return;
	}

	m_state = ConnectionState_e::WAIT_RESET;
	bool succeeded = pDisconnectEx(m_socket, this, TF_REUSE_SOCKET, 0);
	if (!succeeded && WSAGetLastError() != ERROR_IO_PENDING)
	{
		// TODO: Error handling
		LogError("DisconnectEx failed", WSAGetLastError());
	}
}

void IOCPConnection::CompleteReset()
{
	LogInfo("COMPLETE RESET");

	ClearBuffers();
	IssueAccept();
}

void IOCPConnection::ClearBuffers()
{
	ZeroMemory(&m_socketBuffer, MAX_SOCKET_BUFFER_SIZE);
	ZeroMemory(&m_messageBuffer, MAX_MESSAGE_BUFFER_SIZE);
	ZeroMemory(&m_addrBlock, ACCEPT_ADDR_LENGTH * 2);
	m_currentMessageSize = 0;
}

void IOCPConnection::LogError(const char* msg)
{
	printf("ERROR: %s\n", msg);
}

void IOCPConnection::LogError(const char* msg, int errorCode)
{
	char formattedMsg[256];
	sprintf_s(formattedMsg, "%s: Error code %d", msg, errorCode);
	LogError(formattedMsg);
}

void IOCPConnection::LogInfo(const char* msg)
{
	printf("INFO: %s\n", msg);
}