#include "src/runtime/xl6_unicode_internal.h"
#include <string.h>

bool xl6UnicodeScalarV1(int64_t code)
{ return code >= 0 && code <= UINT32_MAX && xrtUnicodeScalar((uint32_t)code); }
xstrview xl6UnicodeCharStringV1(uint32_t scalar)
{
    char encoded[4];
    size_t size = xrtUtf8Encode(scalar, encoded);
    if (!size) { xrtSetErrorKind(XERR_VALUE); return xl6TextFailure; }
    char* data = xrtMalloc(size + 1);
    if (!data) return xl6TextFailure;
    memcpy(data, encoded, size); data[size] = 0;
    return (xstrview){data, size};
}
int64_t xl6UnicodeInvalidTextV1(xstrview text, int64_t size)
{ return xl6TextSelectRaw(&text, size) ? xl6UnicodeInvalid(text) : -1; }
int64_t xl6UnicodeInvalidBytesV1(const xvalue* data, int64_t size)
{
    xstrview text;
    return xl6UnicodeBytes(data, size, &text) ? xl6UnicodeInvalid(text) : -1;
}
static bool xl6UnicodeIndexInput(xstrview* text, int64_t index, int64_t size)
{
    if (index < 0) { xrtSetErrorKind(XERR_RANGE); return false; }
    if (!xl6TextSelectRaw(text, size)) return false;
    /* XRT offset/index scan only the needed prefix. Validate the entire
     * selected value, so an invalid tail cannot yield a plausible index. */
    if (xrtUtf8Valid(*text, NULL)) return true;
    xrtSetErrorKind(XERR_VALUE); return false;
}
int64_t xl6UnicodeByteOffsetV1(xstrview text, int64_t index, int64_t size)
{
    if (!xl6UnicodeIndexInput(&text, index, size)) return -1;
    size_t offset = xrtUtf8Offset(text, (size_t)index);
    return offset == XRT_NPOS ? -1 : (int64_t)offset;
}
int64_t xl6UnicodeScalarIndexV1(xstrview text, int64_t offset, int64_t size)
{
    if (!xl6UnicodeIndexInput(&text, offset, size)) return -1;
    size_t index = xrtUtf8Index(text, (size_t)offset);
    return index == XRT_NPOS ? -1 : (int64_t)index;
}
