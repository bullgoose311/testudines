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

#ifdef _win64

#include <windows.h>

class MessageQueue
{
public:
	MessageQueue();

	void enqueue(connectionId_t connectionId, requestId_t requestId, const char* contents, size_t length, timeout_t timeout);
	void dequeue(message_s& message, timeout_t timeout);

private:
	message_s	m_messages[MESSAGE_QUEUE_CAPACITY];
	size_t		m_queueSize = 0;
	size_t		m_queueFront = 0;
	size_t		m_queueBack = MESSAGE_QUEUE_CAPACITY - 1;
	CRITICAL_SECTION m_criticalSection;
	CONDITION_VARIABLE m_enqueueCvar;
	CONDITION_VARIABLE m_dequeueCvar;

	bool IsFull() { return m_queueSize == MESSAGE_QUEUE_CAPACITY; }
	bool IsEmpty() { return m_queueSize == 0; }
};

#else

	// TODO: Linux message queue interface goes here...

#endif // #ifdef _win64
