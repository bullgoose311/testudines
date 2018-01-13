#include "http.h"
#include "message_queue.h"
#include "os.h"
#include "socket.h"

#include <stdio.h> // printf
#include <Windows.h> // Sleep

bool g_shutDownRequested;
MessageQueue incomingMessageQueue;
MessageQueue* g_incomingMessageQueue = &incomingMessageQueue;
MessageQueue outgoingMessageQueue;
MessageQueue* g_outgoingMessageQueue = &outgoingMessageQueue;

int main(char *argv[], int argc)
{
	if (!OS_Init())
	{
		return 1;
	}

	if (!Sockets_Init())
	{
		return 1;
	}

	if (!HTTP_Init())
	{
		return 1;
	}

	g_shutDownRequested = false;

	printf("Application is running, press ctrl-c to quit...\n");

	// TODO: Move this to a semaphore
	while (!g_shutDownRequested)
	{
		Sleep(100);
	}

	HTTP_Shutdown();

	Sockets_Shutdown();

	OS_Shutdown();

	return 0;
}