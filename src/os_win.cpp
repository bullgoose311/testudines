#ifdef _win64

#include "os.h"
#include "semaphore.h"

#include <iostream> // getch
#include <windows.h>
#include <wincon.h>

extern Semaphore g_shutdownSemaphore;

BOOL WINAPI CtrlHandler(DWORD dwEvent);

bool OS_Init()
{
	if (!SetConsoleCtrlHandler(CtrlHandler, true))
	{
		// TODO: Need error handling
		return false;
	}

	return true;
}

void OS_Shutdown()
{
	if (!SetConsoleCtrlHandler(CtrlHandler, false))
	{
		// TODO: Need error handling
	}

	printf("Application has finished, press enter to exit...\n");
	getchar();
}

BOOL WINAPI CtrlHandler(DWORD dwEvent) 
{
	switch (dwEvent) 
	{
	case CTRL_C_EVENT:      // Falls through..
	case CTRL_CLOSE_EVENT:
		g_shutdownSemaphore.Signal();
		return true;
	default:
		return false;
	}
}

#endif