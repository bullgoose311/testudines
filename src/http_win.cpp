#include "http.h"

#include "common_defines.h"
#include "message_queue.h"
#include "socket.h"
#include "thread_utils.h"

#include <stdio.h>		// printf
#include <windows.h>	// critical section

#define MAX_WORKER_THREADS				16
#define MAX_REQUEST_QUEUE_CAPACITY		1024

extern MessageQueue g_incomingMessageQueue;
extern MessageQueue g_outgoingMessageQueue;

static const timeout_t QUEUE_TIMEOUT = 1000;

static HANDLE			s_hConnectionThreads[MAX_WORKER_THREADS];
static DWORD			s_hConnectionThreadCount;

static DWORD WINAPI WorkerThread(LPVOID WorkContext);
static void LogError(const char* msg);
static void Cleanup();

bool HTTP_Init()
{
	// Initialize thread pool
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

		// TODO: Do threads have access to global functions?
		hThread = CreateThread(NULL, 0, WorkerThread, nullptr, 0, &dwThreadId);
		if (hThread == NULL)
		{
			LogError("Failed to create thread");
			Cleanup();
			return false;
		}

		s_hConnectionThreads[dwCpu] = hThread;
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
		message_s message;
		g_incomingMessageQueue.dequeue(message, QUEUE_TIMEOUT);

		// For now we're just a simple echo server, so echo the message plus a \r\n
		const char END_OF_MSG[] = "\r\n";
		const size_t END_OF_MSG_SIZE = sizeof(END_OF_MSG) / sizeof(*END_OF_MSG);
		strncat_s(message.contents, END_OF_MSG, END_OF_MSG_SIZE);

		g_outgoingMessageQueue.enqueue(message.connectionId, message.contents, message.length + END_OF_MSG_SIZE, QUEUE_TIMEOUT);
	}

	return 0;
}

void LogError(const char* msg)
{
	printf("ERROR: %s", msg);
}

void Cleanup()
{
	for (size_t i = 0; i < s_hConnectionThreadCount; i++)
	{
		CloseHandle(s_hConnectionThreads[i]);
	}
}