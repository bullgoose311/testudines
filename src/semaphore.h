#pragma once

#ifdef _win64

#include <windows.h>

class Semaphore
{
public:
	Semaphore() : Semaphore(1) {}
	Semaphore(unsigned int max);

	void Wait();
	void Signal();

private:
	const unsigned int MAX;

	volatile unsigned int m_currentCount;
	CONDITION_VARIABLE m_waitCvar;
	CONDITION_VARIABLE m_signalCvar;
	CRITICAL_SECTION m_criticalSection;
};

#else

// TODO: Linux semaphore here...

#endif