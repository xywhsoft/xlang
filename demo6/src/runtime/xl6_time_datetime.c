#include "src/runtime/xl6_time_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

void xl6DateTimeDestroyV1(void* handle) { xrtFree(handle); }
void* xl6DateTimeSplitV1(int64_t value, int64_t offset)
{
    xdatetime fields;
    int seconds;
    return xl6TimeComponent(offset, &seconds) && xrtTimeSplitAt(value, seconds, &fields)
        ? xl6DateTimeTake(&fields) : NULL;
}
void* xl6DateTimeFieldsV1(int64_t year, int64_t month, int64_t day,
    int64_t hour, int64_t minute, int64_t second, int64_t microsecond, int64_t offset)
{
    xdatetime fields;
    xtime value;
    if (!xl6TimeFields(&fields, year, month, day, hour, minute, second, microsecond, offset) ||
        !xrtTimeMake(&fields, &value) || !xrtTimeSplitAt(value, fields.Offset, &fields)) return NULL;
    return xl6DateTimeTake(&fields);
}
/* The language borrows a live immutable leaf. Null is only a defensive native
 * call result; getters never clear/replace a caller's pending thread error. */
#define FIELD(Name, Member) int64_t xl6DateTime##Name##V1(void* handle) \
{ return handle ? ((const XL6_DateTime*)handle)->fields.Member : 0; }
FIELD(Year, Year)
FIELD(Month, Month)
FIELD(Day, Day)
FIELD(Hour, Hour)
FIELD(Minute, Minute)
FIELD(Second, Second)
FIELD(Microsecond, Microsecond)
FIELD(Offset, Offset)
FIELD(Weekday, Weekday)
FIELD(DayOfYear, YearDay)
FIELD(IsDST, IsDST)
#undef FIELD
int64_t xl6DateTimeToTimeV1(void* handle)
{
    xtime result = 0;
    if (!handle) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }
    (void)xrtTimeMake(&((const XL6_DateTime*)handle)->fields, &result);
    return result;
}

/* Resident policy: terminal Drop runs after the phased handle is detached. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6DateTimeHandlePolicyV1Get,xl6DateTimeDestroyV1)
