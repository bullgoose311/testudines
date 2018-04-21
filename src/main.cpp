#include "http.h"
#include "log.h"
#include "message_queue.h"
#include "os.h"
#include "semaphore.h"
#include "socket.h"

// TODO: Is this a valid pattern?
#ifdef _win64
#include "semaphore_win.h"
static WinSemaphore s_shutdownSemaphore;
#else
#include "semaphore_linux.h"
static LinuxSemaphore s_shutdownSemaphore;
#endif

Semaphore* g_shutdownSemaphore = (Semaphore*)&s_shutdownSemaphore;

bool g_verboseLogging = true;

int main(int argc, char *argv[])
{
	if (!Log_Init())
	{
		return -1;
	}

	if (!OS_Init())
	{
		return -1;
	}

	if (!Sockets_Init())
	{
		return -1;
	}

	if (!HTTP_Init())
	{
		return -1;
	}

	Log_PrintInfo("Application is running, press ctrl-c to quit...");

	// TODO: Find work for the main thread to do
	g_shutdownSemaphore->Wait();

	HTTP_Shutdown();

	Sockets_Shutdown();

	OS_Shutdown();

	Log_Shutdown();

	return 0;
}