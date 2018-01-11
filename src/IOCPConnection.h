#pragma once

#include "common_defines.h"
#include "common_types.h"

#include <winsock2.h>

#define MAX_SOCKET_BUFFER_SIZE 1024

enum
{
	COMPLETION_KEY_NONE = 0,
	COMPLETION_KEY_IO = 1,
	COMPLETION_KEY_SHUTDOWN = 2
};

enum struct ConnectionState_e
{
	WAIT_ACCEPT,
	WAIT_RECV,
	WAIT_SEND,
	WAIT_RESET
};

class IOCPConnection : public OVERLAPPED
{
public:
	~IOCPConnection();

	bool Initialize(connectionId_t connectionId, SOCKET listenSocket, HANDLE hIOCP);
	connectionId_t GetConnectionId() { return m_connectionId; }
	void OnIocpCompletionPacket(DWORD bytesTransferred);
	bool IssueSend(char* response, messageSize_t responseSize);
	void IssueReset();

private:
	static const int ACCEPT_ADDR_LENGTH = ((sizeof(struct sockaddr_in) + 16)); // dwLocalAddressLength[in] - The number of bytes reserved for the local address information.  This value must be at least 16 bytes more than the maximum address length for the transport protocol in use.

	connectionId_t		m_connectionId = 0;
	HANDLE				m_hIOCP = INVALID_HANDLE_VALUE;
	SOCKET				m_listenSocket = INVALID_SOCKET;
	BYTE				m_addrBlock[ACCEPT_ADDR_LENGTH * 2]; // lpOutputBuffer [in] A pointer to a buffer that receives the first block of data sent on a new connection, the local address of the server, and the remote address of the client.
	SOCKET				m_socket = INVALID_SOCKET;
	ConnectionState_e	m_state = ConnectionState_e::WAIT_ACCEPT;
	char				m_socketBuffer[MAX_SOCKET_BUFFER_SIZE] = { 0 };
	char				m_messageBuffer[MAX_MESSAGE_SIZE] = { 0 };
	size_t				m_currentMessageSize = 0;

	void ClearBuffers();
	void IssueAccept();
	void CompleteAccept();
	void IssueRecv();
	void CompleteRecv(size_t bytesReceived);
	void CompleteSend();
	void CompleteReset();
	void LogError(const char* msg);
	void LogError(const char* msg, int errorCode);
	void LogInfo(const char* msg);
};