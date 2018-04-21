#ifdef _win64

#include "http.h"

#include "common_defines.h"
#include "common_utils.h"
#include "log.h"
#include "message_queue.h"
#include "socket.h"
#include "thread_utils.h"

#include <stdio.h>		// printf
#include <windows.h>	// critical section

#define MAX_WORKER_THREADS				1
#define MAX_REQUEST_QUEUE_CAPACITY		1024

static const timeout_t QUEUE_TIMEOUT = 1000;

static HANDLE			s_httpWorkerThreadHandles[MAX_WORKER_THREADS];
static DWORD			s_httpWorkerThreadCount;

static DWORD WINAPI HttpWorkerThread(LPVOID WorkContext);
static void Cleanup();

bool HTTP_Init()
{
	// Initialize thread pool
	for (int i = 0; i < MAX_WORKER_THREADS; i++)
	{
		s_httpWorkerThreadHandles[i] = INVALID_HANDLE_VALUE;
	}
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	s_httpWorkerThreadCount = systemInfo.dwNumberOfProcessors * 2;
	if (s_httpWorkerThreadCount > MAX_WORKER_THREADS)
	{
		s_httpWorkerThreadCount = MAX_WORKER_THREADS;
	}
	for (DWORD dwCpu = 0; dwCpu < s_httpWorkerThreadCount; dwCpu++)
	{
		HANDLE hThread = INVALID_HANDLE_VALUE;
		DWORD dwThreadId = 0; // We're just tossing this, should we use it elsewhere?

		// TODO: Do threads have access to global functions?
		hThread = CreateThread(NULL, 0, HttpWorkerThread, nullptr, 0, &dwThreadId);
		if (hThread == NULL)
		{
			Log_PrintError("Failed to create thread");
			Cleanup();
			return false;
		}

		s_httpWorkerThreadHandles[dwCpu] = hThread;
		hThread = INVALID_HANDLE_VALUE;
	}

	return true;
}

void HTTP_Shutdown()
{
	Cleanup();
}

DWORD WINAPI HttpWorkerThread(LPVOID context)
{
	while (true)
	{
		message_s message;
		Messages_Dequeue(message, QUEUE_TIMEOUT);

		if (message.connectionId == QUIT_CONNECTION_ID)
		{
			Log_PrintInfo("HTTP worker thread %d shutting down", GetCurrentThreadId());
			break;
		}

		// For now we're just a simple echo server, so echo the message with request ID followed by EOF
		char responseBuffer[MAX_MESSAGE_SIZE];
		int responseMsgSize = sprintf_s(responseBuffer, "%d:%s\r\n", message.requestId, message.contents);

		// TODO: Do we want to block the HTTP thread in the case that the outgoing message buffer is full?
		if (!Sockets_Write(message.connectionId, responseBuffer, responseMsgSize))
		{
			Log_PrintError("Unable to write response for request %d on connection %d", message.requestId, message.connectionId);
		}
	}

	return 0;
}

void Cleanup()
{
	for (size_t i = 0; i < s_httpWorkerThreadCount; i++)
	{
		Messages_Enqueue(QUIT_CONNECTION_ID, 0, "", 0, MESSAGE_QUEUE_TIMEOUT_INFINITE);
	}

	Log_PrintInfo("Waiting for http worker threads to terminate...");
	WaitForMultipleObjects(s_httpWorkerThreadCount, s_httpWorkerThreadHandles, true, INFINITE);

	for (size_t i = 0; i < s_httpWorkerThreadCount; i++)
	{
		CloseHandle(s_httpWorkerThreadHandles[i]);
	}
}

#endif // #ifdef _win64