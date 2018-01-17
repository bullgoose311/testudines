#include "http.h"
#include "message_queue.h"
#include "os.h"
#include "semaphore.h"
#include "socket.h"

#include <stdio.h> // printf

MessageQueue	g_incomingMessageQueue;
MessageQueue	g_outgoingMessageQueue;
Semaphore		g_shutdownSemaphore;

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

	printf("Application is running, press ctrl-c to quit...\n");

	g_shutdownSemaphore.Wait();

	HTTP_Shutdown();

	Sockets_Shutdown();

	OS_Shutdown();

	return 0;
}