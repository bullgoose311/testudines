#pragma once

#include <Windows.h>

#define SCOPED_CRITICAL_SECTION(pCS)			ScopedCriticalSection crit(pCS);

struct ScopedCriticalSection
{
	CRITICAL_SECTION* m_pCriticalSection = nullptr;

	ScopedCriticalSection(CRITICAL_SECTION* pCriticalSection)
	{
		m_pCriticalSection = pCriticalSection;
		EnterCriticalSection(m_pCriticalSection);
	}

	~ScopedCriticalSection()
	{
		LeaveCriticalSection(m_pCriticalSection);
	}
};