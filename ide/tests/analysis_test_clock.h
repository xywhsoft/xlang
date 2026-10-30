#ifndef XLS_ANALYSIS_TEST_CLOCK_H
#define XLS_ANALYSIS_TEST_CLOCK_H
#include <xrt.h>
/* Fixtures keep their original microsecond deadlines across XRT's API
 * migration. Product latency measurements use their own high-resolution clock. */
static inline uint64 xlsTestClockMicros(void)
{
#ifdef XRT_WAIT_FOREVER
    double now=xrtTimer()*1000000.0;
    return now>0 && now<0x1p64 ? (uint64)now : 0;
#else
    return xrtClock();
#endif
}
static inline void xlsTestSleepMicros(uint64 delay)
{
#ifdef XRT_WAIT_FOREVER
    xrtSleep((int64)(delay/1000u+(delay%1000u!=0)));
#else
    xrtSleepUs(delay);
#endif
}
#endif
