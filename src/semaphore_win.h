#pragma once

#ifdef _win64

#include "semaphore.h"

#define _WINSOCKAPI_ // Can't include Windows.h before winsock2.h so we do this here
#include <windows.h>

class WinSemaphore : Semaphore
{
public:
	WinSemaphore() : WinSemaphore(1) {}
	WinSemaphore(unsigned int max);

	virtual void Wait() override;
	virtual void Signal() override;

private:
	const unsigned int MAX;

	volatile unsigned int m_currentCount;
	CONDITION_VARIABLE m_waitCvar;
	CONDITION_VARIABLE m_signalCvar;
	CRITICAL_SECTION m_criticalSection;
};

#endif // _win64