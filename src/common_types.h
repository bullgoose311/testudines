#pragma once

// TODO: Where can I get an unsigned int type?
typedef int connectionId_t;

#ifdef _win64
typedef size_t messageSize_t;
#else
typedef int	messageSize_t;
#endif

const connectionId_t INVALID_CONNECTION_ID = -1;