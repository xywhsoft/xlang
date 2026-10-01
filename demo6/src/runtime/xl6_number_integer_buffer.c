#include "src/runtime/xl6_number_internal.h"
bool xl6NumberBufferIntStringV1(void* handle, int64_t value, int64_t base, int64_t flags)
{
    char data[68]; size_t written = 0;
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT); return false;}
    if (!xl6NumberWriteInt(value, base, flags, data, sizeof(data), &written)) return false;
    return xrtBufferAppend(handle, (xbytesview){(const uint8*)data, written});
}
bool xl6NumberBufferUIntStringV1(void* handle, uint64_t value, int64_t base, int64_t flags)
{
    char data[68]; size_t written = 0;
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT); return false;}
    if (!xl6NumberWriteUInt(value, base, flags, data, sizeof(data), &written)) return false;
    return xrtBufferAppend(handle, (xbytesview){(const uint8*)data, written});
}
