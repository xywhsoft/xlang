#ifndef XL6_UNICODE_INTERNAL_H
#define XL6_UNICODE_INTERNAL_H
#include "xl6_text_internal.h"

/* Byte values borrow their backing allocation only for the native call. The
 * cap is measured in bytes, including NUL; invalid caps never read the input. */
static inline bool xl6UnicodeBytes(const xvalue* value, int64_t size, xstrview* view)
{
    xbytesview bytes;
    if (size < -1) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!xrtValueGetBytes(value, &bytes)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    *view = (xstrview){(const char*)bytes.Data, bytes.Size};
    return xl6TextSelectRaw(view, size);
}
static inline bool xl6UnicodeSpan(xbytesview input,int64_t size,xstrview* output)
{ *output=(xstrview){(const char*)input.Data,input.Size};return xl6TextSelectRaw(output,size); }
static inline int64_t xl6UnicodeInvalid(xstrview text)
{
    size_t error = XRT_NPOS;
    return xrtUtf8Valid(text, &error) ? -1 : (int64_t)error;
}
#endif
