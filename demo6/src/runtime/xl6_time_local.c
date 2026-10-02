#include "src/runtime/xl6_time_internal.h"

void* xl6DateTimeLocalV1(int64_t value, bool local)
{
    xdatetime fields;
    if (!(local ? xrtTimeLocal(value, &fields) : xrtTimeSplit(value, &fields))) return NULL;
    return xl6DateTimeTake(&fields);
}
int64_t xl6TimeFromLocalV1(void* handle, int64_t fold)
{
    xtime result = 0;
    if (!handle) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }
    if (fold < XTIME_FOLD_REJECT || fold > XTIME_FOLD_LATER) {
        xrtSetErrorKind(XERR_RANGE);
        return 0;
    }
    (void)xrtTimeFromLocal(&((const XL6_DateTime*)handle)->fields, (xtimefold)fold, &result);
    return result;
}
int64_t xl6TimeTimezoneOffsetV1(int64_t value)
{
    xdatetime fields;
    return xrtTimeLocal(value, &fields) ? fields.Offset : 0;
}
