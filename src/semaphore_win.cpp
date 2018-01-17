#ifdef _win64

#include "semaphore.h"

Semaphore::Semaphore(unsigned int max) : MAX(max), m_currentCount(0)
{
	InitializeConditionVariable(&m_waitCvar);
	InitializeConditionVariable(&m_signalCvar);
	InitializeCriticalSection(&m_criticalSection);
}

void Semaphore::Wait()
{
	EnterCriticalSection(&m_criticalSection);

	while (m_currentCount == 0)
	{
		SleepConditionVariableCS(&m_waitCvar, &m_criticalSection, INFINITE);
	}

	m_currentCount++;

	LeaveCriticalSection(&m_criticalSection);

	WakeConditionVariable(&m_signalCvar);
}

void Semaphore::Signal()
{
	EnterCriticalSection(&m_criticalSection);

	while (m_currentCount == MAX)
	{
		SleepConditionVariableCS(&m_signalCvar, &m_criticalSection, INFINITE);
	}

	m_currentCount--;

	LeaveCriticalSection(&m_criticalSection);

	WakeConditionVariable(&m_waitCvar);
}

#endif // #ifdef _win64