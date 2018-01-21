#include "http.h"

#include "common_defines.h"
#include "common_utils.h"
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

static HANDLE			s_httpWorkerThreadHandles[MAX_WORKER_THREADS];
static DWORD			s_httpWorkerThreadCount;

static DWORD WINAPI HttpWorkerThread(LPVOID WorkContext);
static void LogInfo(const char* msg);
static void LogError(const char* msg);
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
			LogError("Failed to create thread");
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
		g_incomingMessageQueue.dequeue(message, QUEUE_TIMEOUT);

		if (message.connectionId == QUIT_CONNECTION_ID)
		{
			char msg[256] = { 0 };
			sprintf_s(msg, "HTTP worker thread %d shutting down", GetCurrentThreadId());
			LogInfo(msg);
			break;
		}

		// For now we're just a simple echo server, so echo the message with request ID followed by EOF
		char responseBuffer[MAX_MESSAGE_SIZE];
		int responseMsgSize = sprintf_s(responseBuffer, "%d:%s\r\n", message.requestId, message.contents);

		g_outgoingMessageQueue.enqueue(message.connectionId, message.requestId, responseBuffer, responseMsgSize, MESSAGE_QUEUE_TIMEOUT_INFINITE);
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

void Cleanup()
{
	for (size_t i = 0; i < s_httpWorkerThreadCount; i++)
	{
		g_incomingMessageQueue.enqueue(QUIT_CONNECTION_ID, 0, "", 0, MESSAGE_QUEUE_TIMEOUT_INFINITE);
	}

	LogInfo("Waiting for http worker threads to terminate...");
	WaitForMultipleObjects(s_httpWorkerThreadCount, s_httpWorkerThreadHandles, true, INFINITE);

	for (size_t i = 0; i < s_httpWorkerThreadCount; i++)
	{
		CloseHandle(s_httpWorkerThreadHandles[i]);
	}
}