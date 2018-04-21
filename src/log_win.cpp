#ifdef _win64

#include "log.h"
#include "thread_utils.h"

// TODO: Move va() to a utility function that is platform independent
#include <cstdarg> // va
#include <windows.h> // CRITICAL_SECTION

#define MAX_LOG_LINE_SIZE 256

static CRITICAL_SECTION s_cs;

static void Print(const char* label, const char* fmt, va_list args)
{
	SCOPED_CRITICAL_SECTION(&s_cs);

	printf("%s: ", label);
	vprintf(fmt, args);
	printf("\n");
}

bool Log_Init()
{
	InitializeCriticalSection(&s_cs);

	return true;
}

void Log_PrintInfo(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	Print("INFO", fmt, args);
	va_end(args);
}

void Log_PrintWarning(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	Print("WARNING", fmt, args);
	va_end(args);
}

void Log_PrintError(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	Print("ERROR", fmt, args);
	va_end(args);
}

void Log_Shutdown()
{

}

#endif // #ifdef _win64