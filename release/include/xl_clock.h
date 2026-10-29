#ifndef XL_CLOCK_H
#define XL_CLOCK_H

#include <math.h>
#include <stdint.h>
#include "xrt_decl.h"

/* Absolute language deadlines and relative XRT waits both use milliseconds.
 * UINT64_MAX is the absolute no-deadline marker; relative waits use -1. */
typedef uint64_t XL6_DeadlineV1;
#define XL6_DEADLINE_NEVER UINT64_MAX

static inline uint64_t xl6MonotonicMillisecondsV1(void)
{
    double seconds = xrtTimer();
    if (!isfinite(seconds) || seconds < 0 || seconds >= (double)(UINT64_MAX / UINT64_C(1000))) {
        xrtSetErrorKind(XERR_RANGE);
        return UINT64_MAX;
    }
    return (uint64_t)(seconds * 1000.0);
}

static inline XL6_DeadlineV1 xl6DeadlineAfterMillisecondsV1(uint64_t duration)
{
    uint64_t now = xl6MonotonicMillisecondsV1();
    return duration >= UINT64_MAX - now ? XL6_DEADLINE_NEVER : now + duration;
}

static inline uint64_t xl6DeadlineRemainingV1(XL6_DeadlineV1 deadline)
{
    uint64_t now;
    if (deadline == XL6_DEADLINE_NEVER) return UINT64_MAX;
    now = xl6MonotonicMillisecondsV1();
    return deadline > now ? deadline - now : 0;
}

static inline int64_t xl6DeadlineRemainingMillisecondsV1(XL6_DeadlineV1 deadline)
{
    uint64_t remaining = xl6DeadlineRemainingV1(deadline);
    if (remaining == UINT64_MAX) return XRT_WAIT_FOREVER;
    return remaining > INT64_MAX ? INT64_MAX : (int64_t)remaining;
}

static inline bool xl6DeadlineExpiredV1(XL6_DeadlineV1 deadline)
{
    return deadline != XL6_DEADLINE_NEVER && xl6DeadlineRemainingV1(deadline) == 0;
}

static inline void xl6SleepUntilV1(XL6_DeadlineV1 deadline)
{
    while (!xl6DeadlineExpiredV1(deadline)) {
        int64_t remaining = xl6DeadlineRemainingMillisecondsV1(deadline);
        xrtSleep(remaining == XRT_WAIT_FOREVER ? INT64_MAX : remaining);
    }
}

#endif
