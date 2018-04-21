#pragma once

#ifdef __linux__

#include "semaphore.h"

class LinuxSemaphore : Semaphore
{
public:
	virtual void Wait() override;
	virtual void Signal() override;
};

#endif // #ifdef __linux__