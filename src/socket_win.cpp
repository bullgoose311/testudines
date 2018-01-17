// https://www.codeproject.com/Articles/10330/A-simple-IOCP-Server-Client-Class
// http://www.drdobbs.com/cpp/multithreaded-asynchronous-io-io-comple/201202921?pgno=3
	// - In this example, the completionKey is just an enum specifying whether the operation was an actual IO operation or a SHUTDOWN command
	// - Which actually makes sense looking below now, because really our socket context is just that...a socket

#ifdef _win64

#include "socket.h"

#include "IOCPConnection.h"
#include "message_queue.h"

#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib,"ws2_32")   // Standard socket API

#define MAX_WORKER_THREADS			16
#define MAX_CONCURRENT_CONNECTIONS	64

extern MessageQueue g_outgoingMessageQueue;

static const char* DEFAULT_PORT					= "51983";
static const connectionId_t QUIT_CONNECTION_ID	= -1;

static DWORD WINAPI ConnectionWorkerThread(LPVOID context);
static DWORD WINAPI MessageQueueWorkerThread(LPVOID context);
static void LogInfo(const char* msg);
static void LogError(const char* msg);
static void LogError(const char* msg, int errorCode);
static void Cleanup();

static SOCKET				s_listenSocket = INVALID_SOCKET;
static HANDLE				s_hConnectionThreads[MAX_WORKER_THREADS];
static DWORD				s_hConnectionThreadCount;
static HANDLE				s_hIOCP = INVALID_HANDLE_VALUE;
static IOCPConnection		s_connections[MAX_CONCURRENT_CONNECTIONS];
static HANDLE				s_hMessageQueueThread;

bool Sockets_Init()
{
	// Initialize WSA
	WSADATA s_wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &s_wsaData);
	if (iResult != 0)
	{
		LogError("WSAStartup failed", iResult);
		return false;
	}

	// Initialize IOCP
	s_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (s_hIOCP == NULL)
	{
		Cleanup();
		return false;
	}

	// Initialize connection thread pool
	for (int i = 0; i < MAX_WORKER_THREADS; i++)
	{
		s_hConnectionThreads[i] = INVALID_HANDLE_VALUE;
	}
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	s_hConnectionThreadCount = systemInfo.dwNumberOfProcessors * 2;
	if (s_hConnectionThreadCount > MAX_WORKER_THREADS)
	{
		s_hConnectionThreadCount = MAX_WORKER_THREADS;
	}
	for (DWORD dwCpu = 0; dwCpu < s_hConnectionThreadCount; dwCpu++)
	{
		HANDLE hThread = INVALID_HANDLE_VALUE;
		DWORD dwThreadId = 0; // We're just tossing this, should we use it elsewhere?
		hThread = CreateThread(NULL, 0, ConnectionWorkerThread, s_hIOCP, 0, &dwThreadId);
		if (hThread == NULL)
		{
			LogError("Failed to create thread");
			Cleanup();
			return false;
		}

		s_hConnectionThreads[dwCpu] = hThread;
		hThread = INVALID_HANDLE_VALUE;
	}

	// Initialize message queue thread
	DWORD threadId;
	s_hMessageQueueThread = CreateThread(NULL, 0, MessageQueueWorkerThread, nullptr, 0, &threadId);
	if (s_hMessageQueueThread == NULL)
	{
		LogError("Failed to create message queue worker thread");
		Cleanup();
		return false;
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
		printf("getaddrinfo failed: %d\n", iResult);
		Cleanup();
		return false;
	}

	s_listenSocket = WSASocketW(result->ai_family, result->ai_socktype, result->ai_protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (s_listenSocket == INVALID_SOCKET)
	{
		LogError("WSASocketW failed", WSAGetLastError());
		freeaddrinfo(result);
		Cleanup();
		return false;
	}
	
	iResult = bind(s_listenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		LogError("bind failed", WSAGetLastError());
		freeaddrinfo(result);
		Cleanup();
		return false;
	}

	freeaddrinfo(result);

	iResult = listen(s_listenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		LogError("listen failed", WSAGetLastError());
		Cleanup();
		return false;
	}

	// Associate listen socket with IOCP
	if (CreateIoCompletionPort((HANDLE)s_listenSocket, s_hIOCP, COMPLETION_KEY_IO, 0) != s_hIOCP)
	{
		LogError("failed to associate listen socket with iocp");
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
	for (int i = 0; i < MAX_CONCURRENT_CONNECTIONS; i++)
	{
		if (!s_connections[i].Initialize(i, s_listenSocket, s_hIOCP))
		{
			LogError("Failed to initialize connection");
			Cleanup();
			return false;
		}
	}

	return true;
}

void Sockets_Shutdown()
{
	Cleanup();
}

void Cleanup()
{
	// Stop connection worker threads
	for (size_t i = 0; i < s_hConnectionThreadCount; i++)
	{
		PostQueuedCompletionStatus(s_hIOCP, 0, COMPLETION_KEY_SHUTDOWN, 0);
	}
	LogInfo("Waiting for connection threads to terminate...");
	WaitForMultipleObjects(s_hConnectionThreadCount, s_hConnectionThreads, TRUE, INFINITE);
	for (size_t i = 0; i < s_hConnectionThreadCount; i++)
	{
		CloseHandle(s_hConnectionThreads[i]);
	}

	// Stop message queue thread
	g_outgoingMessageQueue.enqueue(QUIT_CONNECTION_ID, "", 0, MESSAGE_QUEUE_TIMEOUT_INFINITE);
	LogInfo("Waiting for message queue thread to terminate...");
	WaitForSingleObject(s_hMessageQueueThread, INFINITE);
	CloseHandle(s_hMessageQueueThread);

	// Close listen socket
	shutdown(s_listenSocket, SD_BOTH);
	closesocket(s_listenSocket);
	
	// Close IOCP
	CloseHandle(s_hIOCP);

	WSACleanup();
}

DWORD WINAPI ConnectionWorkerThread(LPVOID context)
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

		char msg[256];
		sprintf_s(msg, "Thread %d BLOCKED on GetQueuedCompletionStatus", GetCurrentThreadId());
		LogInfo(msg);

		DWORD bytesTransferred = 0;
		ULONG_PTR completionKey = COMPLETION_KEY_NONE;
		LPOVERLAPPED pOverlapped = nullptr;
		bool bSuccess = GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &completionKey, &pOverlapped, INFINITE);

		ZeroMemory(msg, 256);
		sprintf_s(msg, "Thread %d UNBLOCKED from GetQueuedCompletionStatus", GetCurrentThreadId());
		LogInfo(msg);

		IOCPConnection* pConnection = reinterpret_cast<IOCPConnection*>(pOverlapped);

		if (bSuccess)
		{
			if (completionKey == COMPLETION_KEY_IO)
			{
				pConnection->OnIocpCompletionPacket(bytesTransferred);
			}
			else if (completionKey == COMPLETION_KEY_SHUTDOWN)
			{
				// Allow the thread to terminate
				ZeroMemory(msg, 256);
				sprintf_s(msg, "Connection thread %d shutting down", GetCurrentThreadId());
				LogInfo(msg);
				break;
			}
			else
			{
				// Something's wrong
				ZeroMemory(msg, 256);
				sprintf_s(msg, "Thread %d got an invalid completion key, exiting", GetCurrentThreadId());
				LogError(msg);
				break;
			}
		}
		else
		{
			if (pConnection)
			{
				/*
				- 0, non-null overlapped
					- Data was read / written, but the call failed.  We must clear data in the send / recv queue and close the scoekt
					- socket was likely forcibly closed
				*/

				pConnection->IssueReset();
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

DWORD WINAPI MessageQueueWorkerThread(LPVOID context)
{
	while (true)
	{
		message_s message;
		// TODO: Use semaphore here
		if (!g_outgoingMessageQueue.dequeue(message, MESSAGE_QUEUE_TIMEOUT_INFINITE))
		{
			Sleep(100);
			continue;
		}
		
		if (message.connectionId == QUIT_CONNECTION_ID)
		{
			char msg[256] = { 0 };
			sprintf_s(msg, "Message queue thread %d shutting down", GetCurrentThreadId());
			LogInfo(msg);
			break;
		}

		for (int i = 0; i < MAX_CONCURRENT_CONNECTIONS; i++)
		{
			if (s_connections[i].GetConnectionId() == message.connectionId)
			{
				s_connections[i].IssueSend(message.contents, message.length);
				break;
			}
		}
	}

	return 0;
}

void LogInfo(const char* msg)
{
	printf("INFO: %s\n", msg);
}

void LogError(const char* msg)
{
	printf("ERROR: %s\n", msg);
}

void LogError(const char* msg, int errorCode)
{
	char formattedMsg[256];
	sprintf_s(formattedMsg, "%s, error code %d", msg, errorCode);
	LogError(formattedMsg);
}

#endif