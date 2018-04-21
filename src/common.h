#pragma once

#ifdef _win64

#include <cstdarg>

#define va(fmt, args) 

#else

#define va(fmt, args)

#endif

static void Print(const char* label, const char* fmt, va_list args)
{
	printf("%s: ", label);
	vprintf(fmt, args);
	printf("\n");
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