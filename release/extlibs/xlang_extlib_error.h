#ifndef XLANG_EXTLIB_ERROR_H
#define XLANG_EXTLIB_ERROR_H

/* 轻量扩展库只依赖公开运行时 ABI，不再使用旧 xrtSetError(message, take)。 */
#include "xl_runtime.h"

enum {
    XL6_EXTERR_ARGUMENT = 1,
    XL6_EXTERR_VALUE = 3,
    XL6_EXTERR_MEMORY = 6,
    XL6_EXTERR_IO = 7
};

static void xl6ExtlibError(int kind, const char *domain, const char *message)
{
    xrtSetErrorInfo(kind, domain, 0, message != NULL ? message : "extension failure");
}

#endif
