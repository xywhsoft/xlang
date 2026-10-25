#ifndef XL6_XACME_HOST_COMPAT_H
#define XL6_XACME_HOST_COMPAT_H
/* XACME uses signed milliseconds. The embedded host XRT still uses uint64
 * microseconds (zero = infinite) and absolute microsecond UDP deadlines.
 * Keep this translation local to this implementation, never public macros. */
#include <limits.h>
#include "../upstream/wait-arithmetic.h"
static bool xlacmeTimeout(int64 ms, uint64* us)
{
    if (ms < -1 || ms > (int64)(UINT64_MAX / 1000)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    *us = ms == -1 ? 0 : (uint64)ms * 1000; return true;
}
static xwaitresult xlacmeFutureWait(xfuture* future, int64 ms)
{
    uint64 us;
    if (!xlacmeTimeout(ms, &us)) return XWAIT_ERROR;
    if (!ms) {
        xfuturestate state = xrtFutureState(future);
        return state == XFUTURE_PENDING ? XWAIT_TIMEOUT : XWAIT_OK;
    }
    return xrtFutureWaitFor(future, us);
}
static xfuture* xlacmeNetDial(xnetengine* engine, xnetresolver* resolver,
    cstr host, uint16 port, const xnetdialconfig* config,
    const xnetstreamevents* events, void* data)
{
    xnetdialconfig copy = *config;
    if (!xlacmeTimeout((int64)copy.Timeout, &copy.Timeout)) return NULL;
    return xrtNetDialAsync(engine,resolver,host,port,&copy,events,data);
}
static xfuture* xlacmeTlsDial(xnetengine* engine, xnetresolver* resolver,
    cstr host,uint16 port,const xtlsclientconfig* tls,const xtlsdialconfig* config,
    const xtlsstreamevents* events,void* data)
{
    xtlsdialconfig copy = *config;
    if (!xlacmeTimeout((int64)copy.Timeout, &copy.Timeout)) return NULL;
    return xrtTlsDialAsync(engine,resolver,host,port,tls,&copy,events,data);
}
static xnetudppacket* xlacmeUdpWait(xnetudp* udp,int64 ms,xcancel* cancel)
{
    uint64 us;
    if (!xlacmeTimeout(ms,&us)) return NULL;
    uint64 now = ms == 0 ? xrtClock() : 0;
    return xrtNetUdpReceiveWait(udp,ms == -1 ? 0 :
        (ms == 0 ? (now ? now : 1) : xrtDeadlineAfter(us)),cancel);
}
#define xrtFutureWaitFor xlacmeFutureWait
#define xrtNetDialAsync xlacmeNetDial
#define xrtTlsDialAsync xlacmeTlsDial
#define xrtNetUdpReceiveWait xlacmeUdpWait
#endif
