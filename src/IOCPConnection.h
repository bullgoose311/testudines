#pragma once

#include "common_defines.h"
#include "common_types.h"
#include "IOCPPacketHandler.h"

#include <winsock2.h>

enum
{
	COMPLETION_KEY_NONE = 0,
	COMPLETION_KEY_IO = 1,
	COMPLETION_KEY_SHUTDOWN = 2
};

enum class IOCPConnectionState_e
{
	AWAITING_ACCEPT,
	AWAITING_RESET
};

class IOCPConnection : public IOCPPacketHandler, IOCPStreamClosedListener
{
public:
	IOCPConnection() : IOCPPacketHandler() {}
	~IOCPConnection();

	bool Initialize(connectionId_t connectionId, SOCKET listenSocket, HANDLE hIOCP);
	virtual void OnIocpCompletionPacket(DWORD bytesTransferred);

	virtual void OnStreamClosed() override;

	connectionId_t GetConnectionId() { return m_connectionId; }
	void Send(const char* response, messageSize_t responseSize);

private:
	static const int ACCEPT_ADDR_LENGTH = ((sizeof(struct sockaddr_in) + 16)); // dwLocalAddressLength[in] - The number of bytes reserved for the local address information.  This value must be at least 16 bytes more than the maximum address length for the transport protocol in use.

	connectionId_t			m_connectionId = 0;
	HANDLE					m_hIOCP = INVALID_HANDLE_VALUE;
	SOCKET					m_listenSocket = INVALID_SOCKET;
	BYTE					m_addrBlock[ACCEPT_ADDR_LENGTH * 2]; // lpOutputBuffer [in] A pointer to a buffer that receives the first block of data sent on a new connection, the local address of the server, and the remote address of the client.
	SOCKET					m_socket = INVALID_SOCKET;
	IOCPInputStream			m_recvPacketHandler;
	IOCPOutputStream		m_sendPacketHandler;
	IOCPConnectionState_e	m_state;

	void IssueAccept();
	void CompleteAccept();
	void IssueReset();
	void CompleteReset();
	void LogError(const char* msg);
	void LogError(const char* msg, int errorCode);
	void LogInfo(const char* msg);
};