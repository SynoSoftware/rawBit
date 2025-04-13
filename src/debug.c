#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Keep track of the last message to avoid flooding debug output (optional)
static char lastMessage[1024] = ""; // Increased buffer size slightly
static CRITICAL_SECTION debugCritSec;
static BOOL csInitialized = FALSE;

// Function to initialize the critical section
void InitializeDebugOutput()
{
    if(!csInitialized)
    {
        InitializeCriticalSection(&debugCritSec);
        csInitialized = TRUE;
    }
}

// Function to clean up the critical section
void CleanupDebugOutput()
{
    if(csInitialized)
    {
        DeleteCriticalSection(&debugCritSec);
        csInitialized = FALSE;
    }
}


// Ensure you call InitializeDebugOutput() early in your app (e.g., WinMain start)
// and CleanupDebugOutput() before exiting.

void DebugOut(const char* format, ...)
{
    if(!csInitialized)
    {
        // Optionally attempt initialization here, or just bail/assert
        // For simplicity, we'll just output directly without lock if not initialized
        // but this isn't thread-safe.
        char message_unsafe[1024];
        va_list args_unsafe;
        va_start(args_unsafe, format);
        vsnprintf(message_unsafe, sizeof(message_unsafe) - 1, format, args_unsafe);
        message_unsafe[sizeof(message_unsafe) - 1] = '\0'; // Ensure null termination
        va_end(args_unsafe);
        OutputDebugStringA("DEBUG_LOCK_UNINITIALIZED: ");
        OutputDebugStringA(message_unsafe);
        return;
    }

    char message[1024]; // Use a larger buffer
    va_list args;
    va_start(args, format);
    // Use vsnprintf for safety
    vsnprintf(message, sizeof(message) - 1, format, args);
    message[sizeof(message) - 1] = '\0'; // Ensure null termination
    va_end(args);

    EnterCriticalSection(&debugCritSec);
    // Optional: uncomment the strcmp if you want to suppress duplicate messages
    // if(strcmp(message, lastMessage) != 0)
    // {
    OutputDebugStringA(message);
    // Be careful with strcpy_s buffer sizes if you re-enable lastMessage tracking
    // strcpy_s(lastMessage, sizeof(lastMessage), message);
// }
    LeaveCriticalSection(&debugCritSec);
}