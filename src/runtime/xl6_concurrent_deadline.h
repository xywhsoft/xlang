#ifndef XL6_CONCURRENT_DEADLINE_H
#define XL6_CONCURRENT_DEADLINE_H
/* The pinned native wait clock uses microseconds. Never saturate a finite
 * language timeout into the native UINT64_MAX infinite sentinel. */
static inline bool xl6ConcurrentDeadline(int64_t milliseconds, xdeadline* deadline)
{
    if (milliseconds == -1) { *deadline = XRT_DEADLINE_NEVER; return true; }
    uint64_t now = xrtClock();
    if (milliseconds < 0 || now == UINT64_MAX ||
        (uint64_t)milliseconds > (UINT64_MAX - 1 - now) / 1000u) {
        xrtSetErrorKind(XERR_RANGE);
        return false;
    }
    *deadline = now + (uint64_t)milliseconds * 1000u;
    return true;
}
#endif
