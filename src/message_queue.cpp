#include "message_queue.h"

MessageQueue::MessageQueue()
{
	InitializeCriticalSection(&m_criticalSection);
}

bool MessageQueue::enqueue(connectionId_t connectionId, const char* contents, size_t length, timeout_t timeout)
{
	SCOPED_CRITICAL_SECTION(&m_criticalSection);

	if (IsFull())
	{
		return false;
	}

	m_queueBack = (m_queueBack + 1) % MESSAGE_QUEUE_CAPACITY;
	m_messages[m_queueBack].connectionId = connectionId;
	m_messages[m_queueBack].length = length;
	strncpy_s(m_messages[m_queueBack].contents, contents, length);
	m_queueSize++;

	return true;
}

const message_s* const MessageQueue::dequeue(timeout_t timeout)
{
	SCOPED_CRITICAL_SECTION(&m_criticalSection);

	if (IsEmpty())
	{
		return nullptr;
	}

	message_s* pMessage = &m_messages[m_queueFront];
	m_queueFront = (m_queueFront + 1) % MESSAGE_QUEUE_CAPACITY;
	m_queueSize--;

	return pMessage;
}