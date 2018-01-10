#pragma once

bool HTTP_Init();

bool HTTP_Queue_Request(const char* httpRequest);

void HTTP_Shutdown();