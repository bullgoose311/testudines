#ifdef __linux__

#include "semaphore_linux.h"

static bool s_waiting = true;

void LinuxSemaphore::Wait()
{
	// TODO: Implement semaphore for linux
	while (s_waiting)
	{
	}
}

void LinuxSemaphore::Signal()
{
	// TODO: Implement semaphore for linux
	s_waiting = false;
}

#endif // #ifdef __linux__