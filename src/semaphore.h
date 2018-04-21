#pragma once

class Semaphore
{
public:
	virtual void Wait() = 0;
	virtual void Signal() = 0;
};