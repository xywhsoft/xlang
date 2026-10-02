#include "src/runtime/xl6_time_internal.h"
#include "src/runtime/xl6_time_text_internal.h"

typedef struct XL6_DateTimeTextFormat { const xdatetime* fields; xstrview pattern; } XL6_DateTimeTextFormat;
static size_t fieldsWrite(char* data, size_t capacity, const void* context)
{
    const XL6_DateTimeTextFormat* format = context;
    return xrtDateTimeWrite(data, capacity, format->fields, format->pattern);
}
xstrview xl6DateTimeFormatV1(void* handle, xstrview pattern, int64_t size)
{
    if (!handle) { xrtSetErrorKind(XERR_ARGUMENT); return xl6TextFailure; }
    if (!xl6TextSelect(&pattern, size)) return xl6TextFailure;
    XL6_DateTimeTextFormat format = {&((const XL6_DateTime*)handle)->fields, pattern};
    return xl6TimeTextRender(fieldsWrite, &format);
}
void* xl6DateTimeParseV1(xstrview text, xstrview pattern, int64_t size, int64_t patternSize)
{
    xdatetime fields;
    if (!xl6TextSelect(&text, size) || !xl6TextSelect(&pattern, patternSize) ||
        !xrtDateTimeParse(text, pattern, &fields)) return NULL;
    return xl6DateTimeTake(&fields);
}
