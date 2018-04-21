#pragma once

#include <stdio.h> // printf

bool Log_Init();
void Log_PrintInfo(const char* fmt, ...);
void Log_PrintWarning(const char* fmt, ...);
void Log_PrintError(const char* fmt, ...);
void Log_Shutdown();