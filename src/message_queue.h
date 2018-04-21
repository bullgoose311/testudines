#pragma once

#include "common_defines.h"
#include "common_types.h"

#define MESSAGE_QUEUE_CAPACITY	256

const timeout_t MESSAGE_QUEUE_TIMEOUT_INFINITE = 0;

struct message_s
{
	char			contents[MAX_MESSAGE_SIZE];
	size_t			length;
	connectionId_t	connectionId;
	requestId_t		requestId;
};

bool Messages_Enqueue(connectionId_t connectionId, requestId_t requestId, const char* contents, size_t length, timeout_t timeout);

// TODO: This API leaks implementation details as implementations are expected to block until a message is ready
bool Messages_Dequeue(message_s& message, timeout_t timeout);