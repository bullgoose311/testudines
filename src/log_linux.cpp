#ifdef __linux__

#include "log.h"

bool Log_Init() { return true; }
void Log_PrintInfo(const char* fmt, ...) {}
void Log_PrintWarning(const char* fmt, ...) {}
void Log_PrintError(const char* fmt, ...) {}
void Log_Shutdown() {}

#endif // #ifdef __linux__