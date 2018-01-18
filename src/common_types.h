#pragma once

typedef int connectionId_t;

#ifdef _win64
typedef size_t messageSize_t;
typedef size_t timeout_t;
#else
typedef int	messageSize_t;
typedef int timeout_t;
#endif

const connectionId_t INVALID_CONNECTION_ID = -1;