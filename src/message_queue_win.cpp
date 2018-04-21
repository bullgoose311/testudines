#ifdef _win64

#include "message_queue.h"

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

MessageQueue::MessageQueue()
{
	InitializeCriticalSection(&m_criticalSection);
	InitializeConditionVariable(&m_enqueueCvar);
	InitializeConditionVariable(&m_dequeueCvar);
}

void MessageQueue::enqueue(connectionId_t connectionId, requestId_t requestId, const char* contents, size_t length, timeout_t timeout)
{
	EnterCriticalSection(&m_criticalSection);

	while (IsFull())
	{
		SleepConditionVariableCS(&m_enqueueCvar, &m_criticalSection, INFINITE);
	}

	m_queueBack = (m_queueBack + 1) % MESSAGE_QUEUE_CAPACITY;
	m_messages[m_queueBack].connectionId = connectionId;
	m_messages[m_queueBack].requestId = requestId;
	m_messages[m_queueBack].length = length;
	strncpy_s(m_messages[m_queueBack].contents, contents, length);
	m_queueSize++;

	LeaveCriticalSection(&m_criticalSection);

	WakeConditionVariable(&m_dequeueCvar);
}

void MessageQueue::dequeue(message_s& message, timeout_t timeout)
{
	EnterCriticalSection(&m_criticalSection);

	while (IsEmpty())
	{
		SleepConditionVariableCS(&m_dequeueCvar, &m_criticalSection, INFINITE);
	}

	message_s* pMessage = &m_messages[m_queueFront];
	m_queueFront = (m_queueFront + 1) % MESSAGE_QUEUE_CAPACITY;
	m_queueSize--;

	// We want to re-use this message in the queue, make a copy
	message.connectionId = pMessage->connectionId;
	message.requestId = pMessage->requestId;
	strncpy_s(message.contents, pMessage->contents, pMessage->length);
	message.length = pMessage->length;

	LeaveCriticalSection(&m_criticalSection);

	WakeConditionVariable(&m_enqueueCvar);
}

// NOTE: I had originally thought that implementing a class for each platform was best,
// but that doesn't work because defining the private interface may use platform specific types.
// Therefore this is leftover from the previous implementation but no longer needs to be a class
static MessageQueue	s_incomingMessageQueue;

bool Messages_Enqueue(connectionId_t connectionId, requestId_t requestId, const char* contents, size_t length, timeout_t timeout)
{
	s_incomingMessageQueue.enqueue(connectionId, requestId, contents, length, timeout);
	return true;
}

bool Messages_Dequeue(message_s& message, timeout_t timeout)
{
	s_incomingMessageQueue.dequeue(message, timeout);
	return true;
}

#endif // #ifdef _win64