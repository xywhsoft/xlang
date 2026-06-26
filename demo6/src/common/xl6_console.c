#include "src/common/xl6_console.h"
#include "lib/xrt/xrt.h"

#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <unistd.h>
#endif

#define XL6_CONSOLE_STACK_BUFFER 1024u

#if defined(_WIN32) || defined(_WIN64)
static DWORD xl6ConsoleStdHandle(XL6_ConsoleStream stream)
{
    return stream == XL6_CONSOLE_STDERR ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE;
}

static int xl6ConsoleWriteConsoleW(HANDLE handle, const char* text, size_t size)
{
    u16str wideText;
    size_t wideSize;
    size_t pos;

    wideSize = 0;
    wideText = xrtUTF8to16((u8str)text, size, &wideSize);
    if ( wideText == NULL ) {
        return 0;
    }

    pos = 0;
    while ( pos < wideSize ) {
        DWORD chunk;
        DWORD written;

        chunk = (wideSize - pos) > 0x7fffffffu ? 0x7fffffffU : (DWORD)(wideSize - pos);
        written = 0;
        if ( !WriteConsoleW(handle, wideText + pos, chunk, &written, NULL) || written == 0 ) {
            xrtFree(wideText);
            return 0;
        }
        pos += written;
    }

    xrtFree(wideText);
    return 1;
}

static int xl6ConsoleWriteFile(HANDLE handle, const char* text, size_t size)
{
    const char* cursor;
    size_t remain;

    cursor = text;
    remain = size;
    while ( remain > 0 ) {
        DWORD chunk;
        DWORD written;

        chunk = remain > 0x7fffffffu ? 0x7fffffffU : (DWORD)remain;
        written = 0;
        if ( !WriteFile(handle, cursor, chunk, &written, NULL) || written == 0 ) {
            return 0;
        }
        cursor += written;
        remain -= (size_t)written;
    }
    return 1;
}
#endif

int xl6ConsoleWrite(XL6_ConsoleStream stream, const char* text, size_t size)
{
    if ( text == NULL ) {
        return 1;
    }
    if ( size == (size_t)-1 ) {
        size = strlen(text);
    }
    if ( size == 0 ) {
        return 1;
    }

#if defined(_WIN32) || defined(_WIN64)
    {
        HANDLE handle;
        DWORD mode;

        handle = GetStdHandle(xl6ConsoleStdHandle(stream));
        if ( handle == NULL || handle == INVALID_HANDLE_VALUE ) {
            return 0;
        }

        mode = 0;
        if ( GetConsoleMode(handle, &mode) ) {
            return xl6ConsoleWriteConsoleW(handle, text, size);
        }
        return xl6ConsoleWriteFile(handle, text, size);
    }
#else
    {
        int fd;

        fd = stream == XL6_CONSOLE_STDERR ? 2 : 1;
        while ( size > 0 ) {
            ssize_t written = write(fd, text, size);
            if ( written <= 0 ) {
                return 0;
            }
            text += (size_t)written;
            size -= (size_t)written;
        }
        return 1;
    }
#endif
}

int xl6ConsolePuts(XL6_ConsoleStream stream, const char* text)
{
    return xl6ConsoleWrite(stream, text != NULL ? text : "", (size_t)-1);
}

int xl6ConsoleVPrintf(XL6_ConsoleStream stream, const char* format, va_list args)
{
    char stackBuf[XL6_CONSOLE_STACK_BUFFER];
    char* heapBuf;
    va_list argsCopy;
    int needed;
    int ok;

    if ( format == NULL ) {
        return 0;
    }

    va_copy(argsCopy, args);
    needed = vsnprintf(stackBuf, sizeof(stackBuf), format, argsCopy);
    va_end(argsCopy);
    if ( needed < 0 ) {
        return 0;
    }

    if ( (size_t)needed < sizeof(stackBuf) ) {
        return xl6ConsoleWrite(stream, stackBuf, (size_t)needed);
    }

    heapBuf = (char*)xrtMalloc((size_t)needed + 1u);
    if ( heapBuf == NULL ) {
        return 0;
    }

    va_copy(argsCopy, args);
    needed = vsnprintf(heapBuf, (size_t)needed + 1u, format, argsCopy);
    va_end(argsCopy);
    if ( needed < 0 ) {
        xrtFree(heapBuf);
        return 0;
    }

    ok = xl6ConsoleWrite(stream, heapBuf, (size_t)needed);
    xrtFree(heapBuf);
    return ok;
}

int xl6ConsolePrintf(XL6_ConsoleStream stream, const char* format, ...)
{
    va_list args;
    int ok;

    va_start(args, format);
    ok = xl6ConsoleVPrintf(stream, format, args);
    va_end(args);
    return ok;
}
