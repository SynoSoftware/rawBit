// debug.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void DebugOut(const char* format, ...);
void InitializeDebugOutput();
void CleanupDebugOutput();

#ifdef __cplusplus
}
#endif
