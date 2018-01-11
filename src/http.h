#pragma once

#include "common_types.h"

#ifdef _win64
typedef size_t httpRequestSize_t;
#else
typedef httpRequestSize_t int;
#endif

bool HTTP_Init();

bool HTTP_QueueRequest(connectionId_t connectionId, const char* httpRequest, httpRequestSize_t requestSize);

void HTTP_Shutdown();