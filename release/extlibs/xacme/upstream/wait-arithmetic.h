/* Extracted shared arithmetic from upstream include/xrt/wait.h. */
#define XRT_WAIT_FOREVER INT64_C(-1)
/* Shared arithmetic for independently compiled runtime extensions. */
#include <math.h>
#include <string.h>
#if defined(XRT_FEATURE_WAIT)
/* Private Timer arithmetic. Public waits receive signed milliseconds only. */
static inline uint64 __xrtWaitTimerBits(double Value)
{ uint64 Bits; memcpy(&Bits, &Value, sizeof(Bits)); return Bits; }
static inline double __xrtWaitTimerFromBits(uint64 Bits)
{ double Value; memcpy(&Value, &Bits, sizeof(Value)); return Value; }
static inline void __xrtWaitInvalid(void)
{
    xerrordesc Desc; xerror* Error;
    memset(&Desc, 0, sizeof(Desc));
    Desc.Kind = XERR_ARGUMENT; Desc.Domain = "xrt.wait"; Desc.Code = 1;
    Desc.Operation = "timeout"; Desc.Message = "invalid millisecond timeout or timer value";
    Error = xrtErrorBuild(&Desc);
    if ( Error != NULL ) { xrtSetErrorTake(Error); }
}
static inline double __xrtWaitAfter(int64 Milliseconds)
{
    double Now, Seconds, Result;
    if ( Milliseconds == XRT_WAIT_FOREVER ) { return INFINITY; }
    if ( Milliseconds < 0 ) { __xrtWaitInvalid(); return NAN; }
    Now = xrtTimer();
    if ( !isfinite(Now) ) { return NAN; }
    if ( Milliseconds == 0 ) { return Now; }
    Seconds = (double)(Milliseconds / 1000) + (double)(Milliseconds % 1000) * 0.001;
    /* Round outward, never turning a positive interval into immediate expiry. */
    Result = nextafter(Now + Seconds, INFINITY);
    return Result;
}
static inline int64 __xrtWaitElapsedMs(double Start)
{
    double Elapsed = (xrtTimer() - Start) * 1000.0;
    if (!isfinite(Elapsed)) return 0;
    if (Elapsed <= 0) return 0;
    return Elapsed >= 0x1p63 ? INT64_MAX : (int64)Elapsed;
}
static inline bool __xrtWaitValid(double Limit)
{
    if ( isnan(Limit) || Limit < 0 ) { __xrtWaitInvalid(); return false; }
    return true;
}
static inline bool __xrtWaitExpired(double Limit)
{
    double Now;
    if ( Limit == INFINITY ) { return false; }
    if ( !__xrtWaitValid(Limit) ) { return true; }
    Now = xrtTimer();
    return !isfinite(Now) || Now >= Limit;
}
static inline int64 __xrtWaitRemaining(double Limit)
{
    double Remaining;
    if ( Limit == INFINITY ) { return XRT_WAIT_FOREVER; }
    if ( !__xrtWaitValid(Limit) ) { return -2; }
    Remaining = Limit - xrtTimer();
    if ( !isfinite(Remaining) ) { return -2; }
    if ( Remaining <= 0 ) { return 0; }
    Remaining = ceil(Remaining * 1000.0);
    return Remaining >= 0x1p63 ? INT64_MAX : (int64)Remaining;
}
#endif
