#ifndef XL6_CONSOLE_H
#define XL6_CONSOLE_H

#include <stdarg.h>
#include <stddef.h>

typedef enum XL6_ConsoleStream {
    XL6_CONSOLE_STDOUT = 1,
    XL6_CONSOLE_STDERR = 2
} XL6_ConsoleStream;

int xl6ConsoleWrite(XL6_ConsoleStream stream, const char* text, size_t size);
int xl6ConsolePuts(XL6_ConsoleStream stream, const char* text);
int xl6ConsoleVPrintf(XL6_ConsoleStream stream, const char* format, va_list args);
int xl6ConsolePrintf(XL6_ConsoleStream stream, const char* format, ...);

#endif
