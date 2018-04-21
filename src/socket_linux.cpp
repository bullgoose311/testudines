#ifdef __linux__

#include "socket.h"

bool Sockets_Init() { return true; }

bool Sockets_Write(connectionId_t connectionId, const char* contents, size_t length) { return false; }

void Sockets_Shutdown() { }

#endif