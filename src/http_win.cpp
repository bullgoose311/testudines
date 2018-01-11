#include "http.h"

#include "common_defines.h"
#include "socket.h"

#include <stdio.h>		// printf
#include <windows.h>	// critical section

#define MAX_WORKER_THREADS				16
#define MAX_REQUEST_QUEUE_CAPACITY		1024
#define SCOPED_CRITICAL_SECTION			ScopedCriticalSection crit;

static CRITICAL_SECTION		s_criticalSection;
struct ScopedCriticalSection
{
	ScopedCriticalSection()
	{
		EnterCriticalSection(&s_criticalSection);
	}

	~ScopedCriticalSection()
	{
		LeaveCriticalSection(&s_criticalSection);
	}
};

struct httpRequest_s
{
	connectionId_t	connectionId;
	char			request[MAX_MESSAGE_SIZE];
	messageSize_t	requestSize;
};

static httpRequest_s	s_requestQueue[MAX_REQUEST_QUEUE_CAPACITY];
static size_t			s_currentQueueSize = 0;
static size_t			s_queueFront = 0;
static size_t			s_queueBack = MAX_REQUEST_QUEUE_CAPACITY - 1;
static HANDLE			s_hThreads[MAX_WORKER_THREADS];
static DWORD			s_threadCount;

static DWORD WINAPI WorkerThread(LPVOID WorkContext);
static bool IsQueueFull();
static bool IsQueueEmpty();
static httpRequest_s* DequeueRequest();
static void LogError(const char* msg);
static void Cleanup();

bool HTTP_Init()
{
	// Initialize critical section
	InitializeCriticalSection(&s_criticalSection);

	// Initialize thread pool
	for (int i = 0; i < MAX_WORKER_THREADS; i++)
	{
		s_hThreads[i] = INVALID_HANDLE_VALUE;
	}
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	s_threadCount = systemInfo.dwNumberOfProcessors * 2;
	if (s_threadCount > MAX_WORKER_THREADS)
	{
		s_threadCount = MAX_WORKER_THREADS;
	}
	for (DWORD dwCpu = 0; dwCpu < s_threadCount; dwCpu++)
	{
		HANDLE hThread = INVALID_HANDLE_VALUE;
		DWORD dwThreadId = 0; // We're just tossing this, should we use it elsewhere?

		// TODO: Do threads have access to global functions?
		hThread = CreateThread(NULL, 0, WorkerThread, nullptr, 0, &dwThreadId);
		if (hThread == NULL)
		{
			LogError("Failed to create thread");
			Cleanup();
			return false;
		}

		s_hThreads[dwCpu] = hThread;
		hThread = INVALID_HANDLE_VALUE;
	}

	return true;
}

bool HTTP_QueueRequest(connectionId_t connectionId, const char* httpRequest, httpRequestSize_t requestSize)
{
	SCOPED_CRITICAL_SECTION;

	if (IsQueueFull())
	{
		return false;
	}

	s_queueBack = (s_queueBack + 1) % MAX_REQUEST_QUEUE_CAPACITY;
	s_requestQueue[s_queueBack].connectionId = connectionId;
	s_requestQueue[s_queueBack].requestSize = requestSize;
	strncpy_s(s_requestQueue[s_queueBack].request, httpRequest, requestSize);
	s_currentQueueSize++;

	return true;
}

void HTTP_Shutdown()
{
	Cleanup();
}

DWORD WINAPI WorkerThread(LPVOID context)
{
	while (true)
	{
		// TODO: Make this a semaphore
		httpRequest_s* pHttpRequest = DequeueRequest();
		if (!pHttpRequest)
		{
			Sleep(100);
			continue;
		}

		// For now we're just a simple echo server, so echo the message plus a \r\n
		char response[MAX_MESSAGE_SIZE];
		sprintf_s(response, "%s\r\n", pHttpRequest->request);
		size_t responseSize = pHttpRequest->requestSize + 2;

		// TODO: Decouple HTTP from sockets via a more abstract message queue interface
		if (!Sockets_QueueOutgoingMessage( pHttpRequest->connectionId, response, responseSize))
		{
			// TODO: What to do here...
			LogError("Unable to queue outgoing message");
		}

		pHttpRequest->connectionId = INVALID_CONNECTION_ID;
		ZeroMemory(pHttpRequest->request, MAX_MESSAGE_SIZE);
		pHttpRequest->requestSize = 0;
	}

	return 0;
}

bool IsQueueFull()
{
	return s_currentQueueSize == MAX_REQUEST_QUEUE_CAPACITY;
}

bool IsQueueEmpty()
{
	return s_currentQueueSize == 0;
}

httpRequest_s* DequeueRequest()
{
	SCOPED_CRITICAL_SECTION;

	if (IsQueueEmpty())
	{
		return nullptr;
	}

	httpRequest_s* pHttpRequest = &s_requestQueue[s_queueFront];
	s_queueFront = (s_queueFront + 1) % MAX_REQUEST_QUEUE_CAPACITY;
	s_currentQueueSize--;

	return pHttpRequest;
}

void LogError(const char* msg)
{
	printf("ERROR: %s", msg);
}

void Cleanup()
{
	for (size_t i = 0; i < s_threadCount; i++)
	{
		CloseHandle(s_hThreads[i]);
	}
}