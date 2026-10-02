#ifndef XL6_TIME_TEXT_INTERNAL_H
#define XL6_TIME_TEXT_INTERNAL_H
#include "src/runtime/xl6_text_internal.h"

/* XRT owns all calendar, pattern and protocol rules. Its writer reports the
 * exact required byte count, so no C-string scan or fixed-size buffer is used. */
typedef size_t (*XL6_TimeTextWriter)(char*, size_t, const void*);
static inline xstrview xl6TimeTextRender(XL6_TimeTextWriter write, const void* context)
{
    size_t size = write(NULL, 0, context);
    if (size == XRT_NPOS) return xl6TextFailure;
    if (size > INT64_MAX || size == SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE); return xl6TextFailure;
    }
    char* data = xrtMalloc(size + 1);
    if (!data) return xl6TextFailure;
    size_t written = write(data, size + 1, context);
    if (written == XRT_NPOS) { xrtFree(data); return xl6TextFailure; }
    if (written != size) {
        xrtFree(data); xrtSetErrorKind(XERR_STATE); return xl6TextFailure;
    }
    return (xstrview){data, size};
}
/* A try API parses once. Only the native input-mismatch code is an ordinary
 * false result. UTF-8/size/pattern errors and diagnostic OOM still propagate.
 * Isolate and restore a pre-existing thread error without allocating. */
static inline XL6_ProductBoolTimeV1 xl6TimeTextTryFinish(xerror* saved, bool parsed, xtime value)
{
    if (parsed) { xrtSetErrorTake(saved); return (XL6_ProductBoolTimeV1){true, value}; }
    xerror* error = xrtTakeError();
    if (xrtErrorKind(error) == XERR_VALUE && xrtErrorFind(error, "xrt.time", XTIME_ERROR_PARSE)) {
        xrtErrorFree(error); xrtSetErrorTake(saved);
    } else {
        xrtErrorFree(saved);
        if (error) xrtSetErrorTake(error); else xrtSetErrorKind(XERR_STATE);
    }
    return (XL6_ProductBoolTimeV1){false, 0};
}
#endif
