#pragma once

#include "common_types.h"

bool Sockets_Init();

bool Sockets_Write(connectionId_t connectionId, const char* contents, size_t length);

void Sockets_Shutdown();