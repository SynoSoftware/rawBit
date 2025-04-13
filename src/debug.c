// debug.c
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void DebugOut(const char* format, ...)
{
	static char lastMessage[256] = "";
	char message[256];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	if(strcmp(message, lastMessage) != 0)
	{
		OutputDebugStringA(message);
		strcpy_s(lastMessage, 256, message);
	}
}
