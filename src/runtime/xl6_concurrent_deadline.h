#ifndef XL6_CONCURRENT_DEADLINE_H
#define XL6_CONCURRENT_DEADLINE_H
#include "xl_clock.h"
/* The native and language clocks use milliseconds. Never saturate a finite
 * language timeout into the native UINT64_MAX infinite sentinel. */
static inline bool xl6ConcurrentDeadline(int64_t milliseconds, XL6_DeadlineV1* deadline)
{
    if (milliseconds == -1) { *deadline = XL6_DEADLINE_NEVER; return true; }
    uint64_t now = xl6MonotonicMillisecondsV1();
    if (milliseconds < 0 || now == UINT64_MAX ||
        (uint64_t)milliseconds > UINT64_MAX - 1 - now) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    *deadline = now + (uint64_t)milliseconds;
    return true;
}
#endif
