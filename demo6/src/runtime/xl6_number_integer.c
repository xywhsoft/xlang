#include "src/runtime/xl6_number_internal.h"

static bool xl6NumberParseInt(xstrview text, int64_t base, int64_t flags, int64_t size, int64_t* value)
{
    uint32_t radix, options;
    if (!xl6TextSelectRaw(&text, size) || !xl6NumberU32(base, &radix, "int-parse") ||
        !xl6NumberU32(flags, &options, "int-parse")) return false;
    if (xrtIntParse(text, radix, options, value)) return true;
    xl6NumberParseError(); return false;
}
static bool xl6NumberParseUInt(xstrview text, int64_t base, int64_t flags, int64_t size, uint64_t* value)
{
    uint32_t radix, options;
    if (!xl6TextSelectRaw(&text, size) || !xl6NumberU32(base, &radix, "uint-parse") ||
        !xl6NumberU32(flags, &options, "uint-parse")) return false;
    if (xrtUIntParse(text, radix, options, value)) return true;
    xl6NumberParseError(); return false;
}
int64_t xl6NumberIntParseV1(xstrview text, int64_t base, int64_t flags, int64_t size)
{
    int64_t value = 0;
    (void)xl6NumberParseInt(text, base, flags, size, &value); return value;
}
uint64_t xl6NumberUIntParseV1(xstrview text, int64_t base, int64_t flags, int64_t size)
{
    uint64_t value = 0;
    (void)xl6NumberParseUInt(text, base, flags, size, &value); return value;
}
/* int and explicit int64 have distinct language declaration identities. */
int64_t xl6NumberInt64ParseV1(xstrview text, int64_t base, int64_t flags, int64_t size)
{
    int64_t value = 0;
    (void)xl6NumberParseInt(text, base, flags, size, &value); return value;
}
#define XL6_SIGNED_PARSE(Name, Minimum, Maximum) \
int64_t Name(xstrview text, int64_t base, int64_t flags, int64_t size) \
{ \
    int64_t value = 0; \
    if (!xl6NumberParseInt(text, base, flags, size, &value)) return 0; \
    if (value < Minimum || value > Maximum) { \
        xl6NumberError(XERR_RANGE, XNUMBER_ERROR_RANGE, #Name, "integer exceeds target width"); return 0; \
    } \
    return value; \
}
#define XL6_UNSIGNED_PARSE(Name, Maximum) \
int64_t Name(xstrview text, int64_t base, int64_t flags, int64_t size) \
{ \
    uint64_t value = 0; \
    if (!xl6NumberParseUInt(text, base, flags, size, &value)) return 0; \
    if (value > Maximum) { \
        xl6NumberError(XERR_RANGE, XNUMBER_ERROR_RANGE, #Name, "integer exceeds target width"); return 0; \
    } \
    return (int64_t)value; \
}
XL6_SIGNED_PARSE(xl6NumberInt8ParseV1, INT8_MIN, INT8_MAX)
XL6_SIGNED_PARSE(xl6NumberInt16ParseV1, INT16_MIN, INT16_MAX)
XL6_SIGNED_PARSE(xl6NumberInt32ParseV1, INT32_MIN, INT32_MAX)
XL6_UNSIGNED_PARSE(xl6NumberUInt8ParseV1, UINT8_MAX)
XL6_UNSIGNED_PARSE(xl6NumberUInt16ParseV1, UINT16_MAX)
XL6_UNSIGNED_PARSE(xl6NumberUInt32ParseV1, UINT32_MAX)
#undef XL6_SIGNED_PARSE
#undef XL6_UNSIGNED_PARSE

xstrview xl6NumberIntStringV1(int64_t value, int64_t base, int64_t flags)
{
    char data[68]; size_t written = 0;
    if (!xl6NumberWriteInt(value, base, flags, data, sizeof(data), &written)) return xl6TextFailure;
    return xl6NumberTextCopy(data, written);
}
xstrview xl6NumberUIntStringV1(uint64_t value, int64_t base, int64_t flags)
{
    char data[68]; size_t written = 0;
    if (!xl6NumberWriteUInt(value, base, flags, data, sizeof(data), &written)) return xl6TextFailure;
    return xl6NumberTextCopy(data, written);
}
