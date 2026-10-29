#include "src/runtime/xl6_time_text_internal.h"

typedef struct XL6_TimeTextFormat { xtime value; int offset; xstrview pattern; } XL6_TimeTextFormat;
static size_t patternWrite(char* data, size_t capacity, const void* context)
{
    const XL6_TimeTextFormat* format = context;
    return xrtTimeWrite(data, capacity, format->value, format->offset, format->pattern);
}
static size_t rfcWrite(char* data, size_t capacity, const void* context)
{
    const XL6_TimeTextFormat* format = context;
    return xrtTimeWriteRFC3339(data, capacity, format->value, format->offset);
}
static size_t httpWrite(char* data, size_t capacity, const void* context)
{ return xrtTimeWriteHTTPDate(data, capacity, ((const XL6_TimeTextFormat*)context)->value); }
static bool offsetCheck(int64_t offset, int* result)
{
    if (offset < INT_MIN || offset > INT_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
    *result = (int)offset; return true;
}
xstrview xl6TimeFormatV1(int64_t value, xstrview pattern, int64_t offset, int64_t size)
{
    XL6_TimeTextFormat format = {value, 0, pattern};
    if (!xl6TextSelect(&format.pattern, size) || !offsetCheck(offset, &format.offset)) return xl6TextFailure;
    return xl6TimeTextRender(patternWrite, &format);
}
xstrview xl6TimeRFC3339V1(int64_t value, int64_t offset)
{
    XL6_TimeTextFormat format = {value, 0, {NULL, 0}};
    if (!offsetCheck(offset, &format.offset)) return xl6TextFailure;
    return xl6TimeTextRender(rfcWrite, &format);
}
xstrview xl6TimeHTTPDateV1(int64_t value)
{
    XL6_TimeTextFormat format = {value, 0, {NULL, 0}};
    return xl6TimeTextRender(httpWrite, &format);
}
int64_t xl6TimeParsePatternV1(xstrview text, xstrview pattern, int64_t size, int64_t patternSize)
{
    xtime value = 0;
    if (xl6TextSelect(&text, size) && xl6TextSelect(&pattern, patternSize))
        (void)xrtTimeParse(text, pattern, &value);
    return value;
}
XL6_ProductBoolTimeV1 xl6TimeTryParsePatternV1(xstrview text, xstrview pattern, int64_t size, int64_t patternSize)
{
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&pattern, patternSize))
        return (XL6_ProductBoolTimeV1){false, 0};
    xerror* saved = xrtTakeError();
    xtime value = 0;
    bool parsed = xrtTimeParse(text, pattern, &value);
    return xl6TimeTextTryFinish(saved, parsed, value);
}
/* Different protocol parsers share only their physical declaration shape. */
#define PARSE(Name, Native) \
int64_t xl6TimeParse##Name##V1(xstrview text, int64_t size) \
{ xtime value = 0; if (xl6TextSelect(&text, size)) (void)Native(text, &value); return value; } \
XL6_ProductBoolTimeV1 xl6TimeTryParse##Name##V1(xstrview text, int64_t size) \
{ \
    if (!xl6TextSelect(&text, size)) return (XL6_ProductBoolTimeV1){false, 0}; \
    xerror* saved = xrtTakeError(); xtime value = 0; \
    bool parsed = Native(text, &value); \
    return xl6TimeTextTryFinish(saved, parsed, value); \
}
PARSE(Any, xrtTimeParseAny)
PARSE(RFC3339, xrtTimeParseRFC3339)
#undef PARSE
int64_t xl6TimeParseHTTPDateV1(xstrview text, int64_t size)
{
    xtime value = 0;
    if (xl6TextSelect(&text, size)) (void)xrtTimeParseHTTPDate(text, &value);
    return value;
}
XL6_ProductBoolTimeV1 xl6TimeTryParseHTTPDateV1(xstrview text, int64_t size)
{
    xtime value = 0;
    if (!xl6TextSelect(&text, size)) return (XL6_ProductBoolTimeV1){false, 0};
    /* Native HTTP try parsing neither allocates nor touches thread errors. */
    bool parsed = xrtTimeTryParseHTTPDate(text, &value);
    return (XL6_ProductBoolTimeV1){parsed, parsed ? value : 0};
}
