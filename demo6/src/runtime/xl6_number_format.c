#include "src/runtime/xl6_text_internal.h"

/* The XRT formatter owns the sole implementation and already knows the exact
 * result size. Transfer that allocation, never measure it as a C string. */
xstrview xl6NumberIntFormatV1(int64_t value, xstrview format, int64_t size)
{
    size_t written = 0;
    if (!xl6TextSelectRaw(&format, size)) return xl6TextFailure;
    char* data = xrtIntFormatSized(value, format, &written);
    return xl6TextOwn(data, written);
}
xstrview xl6NumberUIntFormatV1(uint64_t value, xstrview format, int64_t size)
{
    size_t written = 0;
    if (!xl6TextSelectRaw(&format, size)) return xl6TextFailure;
    char* data = xrtUIntFormatSized(value, format, &written);
    return xl6TextOwn(data, written);
}
xstrview xl6NumberFloatFormatV1(double value, xstrview format, int64_t size)
{
    size_t written = 0;
    if (!xl6TextSelectRaw(&format, size)) return xl6TextFailure;
    char* data = xrtNumFormatSized(value, format, &written);
    return xl6TextOwn(data, written);
}
