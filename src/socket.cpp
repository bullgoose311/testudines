#include "socket.h"

#ifdef __linux__
	#include "socket_linux.h"
#elif _win64
	#include "socket_win.h"
#endif

bool Sockets_Init()
{
	return Sockets_Platform_Init();
}

void Sockets_Shutdown()
{
	return Sockets_Platform_Shutdown();
}