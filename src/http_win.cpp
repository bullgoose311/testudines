#include "http.h"

#include "common_defines.h"
#include "message_queue.h"
#include "socket.h"
#include "thread_utils.h"

#include <stdio.h>		// printf
#include <windows.h>	// critical section

#define MAX_WORKER_THREADS				16
#define MAX_REQUEST_QUEUE_CAPACITY		1024

extern MessageQueue* g_incomingMessageQueue;

static const timeout_t QUEUE_TIMEOUT = 1000;

static HANDLE			s_hThreads[MAX_WORKER_THREADS];
static DWORD			s_threadCount;

static DWORD WINAPI WorkerThread(LPVOID WorkContext);
static void LogError(const char* msg);
static void Cleanup();

bool HTTP_Init()
{
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

void HTTP_Shutdown()
{
	Cleanup();
}

DWORD WINAPI WorkerThread(LPVOID context)
{
	while (true)
	{
		// TODO: Make this a semaphore
		const message_s* pHttpRequest = g_incomingMessageQueue->dequeue(QUEUE_TIMEOUT);
		if (!pHttpRequest)
		{
			Sleep(100);
			continue;
		}

		// For now we're just a simple echo server, so echo the message plus a \r\n
		char response[MAX_MESSAGE_SIZE];
		sprintf_s(response, "%s\r\n", pHttpRequest->contents);
		size_t responseSize = pHttpRequest->length + 2;

		// TODO: Decouple HTTP from sockets via a more abstract message queue interface
		if (!Sockets_QueueOutgoingMessage( pHttpRequest->connectionId, response, responseSize))
		{
			// TODO: What to do here...
			LogError("Unable to queue outgoing message");
		}
	}

	return 0;
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