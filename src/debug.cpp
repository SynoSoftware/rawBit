#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Keep track of the last message to avoid flooding debug output (optional)
static char lastMessage[1024] = ""; // Increased buffer size slightly
static CRITICAL_SECTION debugCritSec;
static BOOL csInitialized = FALSE;

#ifdef __cplusplus
extern "C" {
#endif

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
    // If the critical section wasn't initialized, output a warning and the message directly (unsafe)
    if(!csInitialized)
    {
        char message_unsafe[1024];
        va_list args_unsafe;
        va_start(args_unsafe, format);
        vsnprintf_s(message_unsafe, sizeof(message_unsafe), _TRUNCATE, format, args_unsafe); // Use safer vsnprintf_s
        va_end(args_unsafe);
        OutputDebugStringA("DEBUG_LOCK_UNINITIALIZED: ");
        OutputDebugStringA(message_unsafe);
        OutputDebugStringA("\n"); // Add newline for clarity
        return;
    }

    char message[1024]; // Buffer for the current message
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args); // Create the message string safely
    va_end(args);

    EnterCriticalSection(&debugCritSec); // Lock for thread safety

    // --- ADDED/RESTORED Duplicate Check ---
    if(strcmp(message, lastMessage) != 0)
    {
        // Only output if the message is DIFFERENT from the last one
        OutputDebugStringA(message);
        OutputDebugStringA("\n"); // Add newline after message

        // Update the last message tracker
        // strcpy_s is safer than strcpy
        strcpy_s(lastMessage, sizeof(lastMessage), message);
    }
    // --- End Duplicate Check ---

    LeaveCriticalSection(&debugCritSec); // Unlock
}

#ifdef __cplusplus
}
#endif
