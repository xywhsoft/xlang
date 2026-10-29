#ifndef XL6_TIME_INTERNAL_H
#define XL6_TIME_INTERNAL_H
#include "xl6_text_value.h"
#include <limits.h>

/* An immutable native leaf: no language references, borrowed buffers, host
 * timezone state, or thread affinity. All calendar rules remain in XRT. */
typedef struct XL6_DateTime { xdatetime fields; } XL6_DateTime;
static inline bool xl6TimeComponent(int64_t value, int* result)
{
    if (value < INT_MIN || value > INT_MAX) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    *result = (int)value;
    return true;
}
static inline bool xl6TimeFields(xdatetime* fields, int64_t year, int64_t month,
    int64_t day, int64_t hour, int64_t minute, int64_t second,
    int64_t millisecond, int64_t offset)
{
    *fields = (xdatetime){0};
    fields->Year = year;
    return xl6TimeComponent(month, &fields->Month) && xl6TimeComponent(day, &fields->Day) &&
        xl6TimeComponent(hour, &fields->Hour) && xl6TimeComponent(minute, &fields->Minute) &&
        xl6TimeComponent(second, &fields->Second) && xl6TimeComponent(millisecond, &fields->Millisecond) &&
        xl6TimeComponent(offset, &fields->Offset);
}
static inline void* xl6DateTimeTake(const xdatetime* fields)
{
    XL6_DateTime* value = xrtMalloc(sizeof(*value));
    if (value) value->fields = *fields;
    return value;
}
#endif
