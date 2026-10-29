#include "src/runtime/xl6_number_internal.h"

double xl6NumberFloatParseV1(xstrview text, int64_t flags, int64_t size)
{
    double value = 0; uint32_t options;
    if (!xl6TextSelectRaw(&text, size) || !xl6NumberU32(flags, &options, "num-parse")) return 0;
    if (xrtNumParse(text, options, &value)) return value;
    xl6NumberParseError(); return 0;
}
xstrview xl6NumberFloatStringV1(double value, int64_t flags)
{
    char data[40]; size_t written = 0;
    if (!xl6NumberWriteFloat(value, flags, data, sizeof(data), &written)) return xl6TextFailure;
    return xl6NumberTextCopy(data, written);
}
