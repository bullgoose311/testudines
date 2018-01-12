#pragma once

#include "common_defines.h"
#include "common_types.h"
#include "thread_utils.h"

#define MESSAGE_QUEUE_CAPACITY	256

struct message_s
{
	char			contents[MAX_MESSAGE_SIZE];
	size_t			length;
	connectionId_t	connectionId;
};

class MessageQueue
{
public:
	MessageQueue();

	bool enqueue(connectionId_t connectionId, const char* contents, size_t length, timeout_t timeout);
	const message_s* const dequeue(timeout_t timeout);

private:
	message_s	m_messages[MESSAGE_QUEUE_CAPACITY];
	size_t		m_queueSize = 0;
	size_t		m_queueFront = 0;
	size_t		m_queueBack = MESSAGE_QUEUE_CAPACITY - 1;

	bool IsFull() { return m_queueSize == MESSAGE_QUEUE_CAPACITY; }
	bool IsEmpty() { return m_queueSize == 0; }
	CRITICAL_SECTION m_criticalSection;
};