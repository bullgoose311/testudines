#ifdef _win64

#include "semaphore_win.h"

WinSemaphore::WinSemaphore(unsigned int max) : MAX(max), m_currentCount(0)
{
	InitializeConditionVariable(&m_waitCvar);
	InitializeConditionVariable(&m_signalCvar);
	InitializeCriticalSection(&m_criticalSection);
}

void WinSemaphore::Wait()
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

void WinSemaphore::Signal()
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