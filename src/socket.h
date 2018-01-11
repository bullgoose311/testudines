#pragma once

#include "common_types.h"

bool Sockets_Init();

// TODO: Figure out why I can't pass const char* here...
bool Sockets_QueueOutgoingMessage(connectionId_t connectionId, char* message, messageSize_t messageSize);

void Sockets_Shutdown();