#include "src/common/xl6_console.h"
#include "lib/xrt/xrt.h"

#include <string.h>

#define XL6_CONSOLE_STACK_BUFFER 1024u

void xl6ConsoleSessionBegin(void)
{
    /*
     * 控制台输出由 XRT 逐次处理，不修改进程共享的控制台代码页。
     * 保留会话接口可让 CLI 与 DLL 生命周期调用点维持简单、稳定。
     */
}

void xl6ConsoleSessionEnd(void)
{
}

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

    return xrtConsoleWrite(stream == XL6_CONSOLE_STDERR
                               ? XRT_CONSOLE_STDERR
                               : XRT_CONSOLE_STDOUT,
                           text,
                           size)
               ? 1
               : 0;
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
