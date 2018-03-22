#ifdef _win64

#include "message_queue.h"

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

#endif // #ifdef _win64