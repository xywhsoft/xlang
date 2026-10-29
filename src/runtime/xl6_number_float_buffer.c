#include "src/runtime/xl6_number_internal.h"
bool xl6NumberBufferFloatStringV1(void* handle, double value, int64_t flags)
{
    char data[40]; size_t written = 0;
    if (!handle) {xrtSetErrorKind(XERR_ARGUMENT); return false;}
    if (!xl6NumberWriteFloat(value, flags, data, sizeof(data), &written)) return false;
    return xrtBufferAppend(handle, (xbytesview){(const uint8*)data, written});
}
