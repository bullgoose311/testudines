#ifdef __linux__

#include "message_queue.h"

bool Messages_Enqueue(connectionId_t connectionId, requestId_t requestId, const char* contents, size_t length, timeout_t timeout)
{
	// TODO: Implement thread safe message queue in linux
	return false;
}

bool Messages_Dequeue(message_s& message, timeout_t timeout)
{
	// TODO: Implement thread safe message queue in linux
	return false;
}

#endif // #ifdef __linux__