#include "src/runtime/xl6_text_internal.h"

/* Format into one independent allocation before mutating the buffer. This
 * makes aliasing inputs safe and keeps the logical bytes unchanged on either
 * formatting or growth failure. No intermediate xvalue/string conversion. */
static bool xl6NumberAppend(xbuffer* buffer, xstrview text)
{
    if (!text.Data) return false;
    bool ok = xrtBufferAppend(buffer, (xbytesview){(const uint8*)text.Data, text.Size});
    xrtFree((void*)text.Data);
    return ok;
}
bool xl6NumberBufferIntFormatV1(void* buffer, int64_t value, xstrview format, int64_t size)
{
    if (!buffer) {xrtSetErrorKind(XERR_ARGUMENT); return false;}
    return xl6NumberAppend(buffer, xl6NumberIntFormatV1(value, format, size));
}
bool xl6NumberBufferUIntFormatV1(void* buffer, uint64_t value, xstrview format, int64_t size)
{
    if (!buffer) {xrtSetErrorKind(XERR_ARGUMENT); return false;}
    return xl6NumberAppend(buffer, xl6NumberUIntFormatV1(value, format, size));
}
bool xl6NumberBufferFloatFormatV1(void* buffer, double value, xstrview format, int64_t size)
{
    if (!buffer) {xrtSetErrorKind(XERR_ARGUMENT); return false;}
    return xl6NumberAppend(buffer, xl6NumberFloatFormatV1(value, format, size));
}
