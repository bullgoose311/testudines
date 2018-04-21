#ifdef _win64

#include "socket.h"

#include "common_utils.h"
#include "IOCPConnection.h"
#include "log.h"
#include "message_queue.h"

#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib,"ws2_32")   // Standard socket API

#define MAX_IOCP_WORKER_THREADS	16
#define MAX_CONCURRENT_CONNECTIONS	1024

static const char* DEFAULT_PORT	= "51983";

static DWORD WINAPI IocpWorkerThread(LPVOID context);
static void Cleanup();

static SOCKET				s_listenSocket = INVALID_SOCKET;
static HANDLE				s_iocpWorkerThreadHandles[MAX_IOCP_WORKER_THREADS];
static DWORD				s_iocpWorkerThreadCount;
static HANDLE				s_hIOCP = INVALID_HANDLE_VALUE;
static IOCPConnection		s_connections[MAX_CONCURRENT_CONNECTIONS];

bool Sockets_Init()
{
	// Initialize WSA
	WSADATA s_wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &s_wsaData);
	if (iResult != 0)
	{
		Log_PrintError("WSAStartup failed", iResult);
		return false;
	}

	// Initialize IOCP
	s_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (s_hIOCP == NULL)
	{
		Cleanup();
		return false;
	}

	// Initialize IOCP worker threads
	for (int i = 0; i < MAX_IOCP_WORKER_THREADS; i++)
	{
		s_iocpWorkerThreadHandles[i] = INVALID_HANDLE_VALUE;
	}
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	s_iocpWorkerThreadCount = systemInfo.dwNumberOfProcessors * 2;
	if (s_iocpWorkerThreadCount > MAX_IOCP_WORKER_THREADS)
	{
		s_iocpWorkerThreadCount = MAX_IOCP_WORKER_THREADS;
	}
	for (DWORD dwCpu = 0; dwCpu < s_iocpWorkerThreadCount; dwCpu++)
	{
		HANDLE hThread = INVALID_HANDLE_VALUE;
		DWORD dwThreadId = 0; // We're just tossing this, should we use it elsewhere?
		hThread = CreateThread(NULL, 0, IocpWorkerThread, s_hIOCP, 0, &dwThreadId);
		if (hThread == NULL)
		{
			Log_PrintError("Failed to create thread");
			Cleanup();
			return false;
		}

		s_iocpWorkerThreadHandles[dwCpu] = hThread;
		hThread = INVALID_HANDLE_VALUE;
	}

	// Create listen socket
	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family		= AF_INET;		// IPv4
	hints.ai_socktype	= SOCK_STREAM;	// Stream
	hints.ai_protocol	= IPPROTO_IP;	// IP ????
	hints.ai_flags		= AI_PASSIVE;	// We're going to bind to this socket
	addrinfo *result;
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		Log_PrintError("getaddrinfo failed: %d", iResult);
		Cleanup();
		return false;
	}

	s_listenSocket = WSASocketW(result->ai_family, result->ai_socktype, result->ai_protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (s_listenSocket == INVALID_SOCKET)
	{
		Log_PrintError("WSASocketW failed", WSAGetLastError());
		freeaddrinfo(result);
		Cleanup();
		return false;
	}
	
	iResult = bind(s_listenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		Log_PrintError("bind failed: %d", WSAGetLastError());
		freeaddrinfo(result);
		Cleanup();
		return false;
	}

	freeaddrinfo(result);

	iResult = listen(s_listenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		Log_PrintError("listen failed: %d", WSAGetLastError());
		Cleanup();
		return false;
	}

	// Associate listen socket with IOCP
	if (CreateIoCompletionPort((HANDLE)s_listenSocket, s_hIOCP, COMPLETION_KEY_IO, 0) != s_hIOCP)
	{
		Log_PrintError("failed to associate listen socket with iocp");
		Cleanup();
		return false;
	}

	/* We can reduce cpu by not buffering twice, the following tells winsock to send directly from our buffers
	int zero = 0;
	iResult = setsockopt(g_listenSocket, SOL_SOCKET, SO_SNDBUF, (char *)&zero, sizeof(zero));
	if (iResult == SOCKET_ERROR)
	{
		Cleanup();
		printf("Error at setsockopt(): %d\n", WSAGetLastError());
		return false;
	}
	*/

	// Initialize the connections
	for (int i = MAX_CONCURRENT_CONNECTIONS - 1; i >= 0; --i)
	{
		if (!s_connections[i].Initialize(i, s_listenSocket, s_hIOCP))
		{
			Log_PrintError("Failed to initialize connection");
			Cleanup();
			return false;
		}
	}

	return true;
}

bool Sockets_Write(connectionId_t connectionId, const char* buffer, size_t size)
{
	for (int i = 0; i < MAX_CONCURRENT_CONNECTIONS; i++)
	{
		if (s_connections[i].GetConnectionId() == connectionId)
		{
			return s_connections[i].GetOutputStream()->Write(buffer, size);
		}
	}

	return false;
}

void Sockets_Shutdown()
{
	Cleanup();
}

void Cleanup()
{
	// Stop iocp worker threads
	for (size_t i = 0; i < s_iocpWorkerThreadCount; i++)
	{
		PostQueuedCompletionStatus(s_hIOCP, 0, COMPLETION_KEY_SHUTDOWN, 0);
	}
	Log_PrintInfo("Waiting for iocp threads to terminate...");
	WaitForMultipleObjects(s_iocpWorkerThreadCount, s_iocpWorkerThreadHandles, TRUE, INFINITE);
	for (size_t i = 0; i < s_iocpWorkerThreadCount; i++)
	{
		CloseHandle(s_iocpWorkerThreadHandles[i]);
	}

	// Close listen socket
	shutdown(s_listenSocket, SD_BOTH);
	closesocket(s_listenSocket);
	
	// Close IOCP
	CloseHandle(s_hIOCP);

	WSACleanup();
}

DWORD WINAPI IocpWorkerThread(LPVOID context)
{
	HANDLE hIOCP = (HANDLE)context;
	
	while (true)
	{
		// Here's where we block
		/*
		GetQueuedCompletionStatus return codes
		- 0, null overlapped
			- Immediate fail, parameters are wrong
		- 0, non-null overlapped
			- Data was read / written, but the call failed.  We must clear data in the send / recv queue and close the scoekt
			- socket was likely forcibly closed
		- non-zero, null overlapped
			- should never happen
		- non-zero, non-null overlapped, zero bytes
			- socket / file handle was closed cleanly
		- non-zero, non-null, non-zero bytes
			- bytes were transferred into the block pointed to by overlapped, the direction is up to us to remember by stashing data in the overlapped structure
		*/

		DWORD bytesTransferred = 0;
		ULONG_PTR completionKey = COMPLETION_KEY_NONE;
		LPOVERLAPPED pOverlapped = nullptr;
		bool bSuccess = GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &completionKey, &pOverlapped, INFINITE);
		IOCPPacketHandler* pPacketHandler = static_cast<IOCPPacketHandler*>(pOverlapped);
		if (bSuccess)
		{
			if (completionKey == COMPLETION_KEY_IO)
			{
				pPacketHandler->OnIocpCompletionPacket(bytesTransferred);
			}
			else if (completionKey == COMPLETION_KEY_SHUTDOWN)
			{
				// Allow the thread to terminate
				Log_PrintInfo("Connection thread %d shutting down", GetCurrentThreadId());
				break;
			}
			else
			{
				// Something's wrong
				Log_PrintError("Thread %d got an invalid completion key, exiting", GetCurrentThreadId());
				break;
			}
		}
		else
		{
			if (pPacketHandler)
			{
				/*
				- 0, non-null overlapped
					- Data was read / written, but the call failed.  We must clear data in the send / recv queue and close the scoekt
					- socket was likely forcibly closed
				*/

				// TODO: What do we actually want to happen here...we don't want a call to DisconnectEx() I don't think
				pPacketHandler->OnIocpError();
			}
			else
			{
				/*
				- 0, null overlapped
					- Immediate fail, parameters are wrong
				*/

				// TODO: Error handling, we need to exit the program here

				break;
			}
		}
	}

	return 0;
}

#endif