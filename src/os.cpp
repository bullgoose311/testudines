#include "os.h"

#ifdef __linux__
#include "os_linux.h"
#elif _win64
#include "os_win.h"
#endif

bool OS_Init()
{
	return OS_Platform_Init();
}

void OS_Shutdown()
{
	OS_Platform_Shutdown();
}