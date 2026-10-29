#ifndef XL6_CHANNEL_WAIT_COMMON_H
#define XL6_CHANNEL_WAIT_COMMON_H
#include "xl6_channel_internal.h"
#include "xl6_concurrent_deadline.h"
#include <stdlib.h>
static inline bool xl6ChannelWaitDeadline(XL6_ChannelWaitOptionsV2 options,xdeadline* deadline)
{
    if (options.size!=sizeof(options) || options.mode>XL6_CHANNEL_UNTIL_V2 ||
        (options.mode!=XL6_CHANNEL_FOR_V1 && options.timeoutMs) ||
        (options.mode!=XL6_CHANNEL_UNTIL_V2 && options.deadlineUs)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    if (options.mode==XL6_CHANNEL_FOR_V1) {
        if (options.timeoutMs<0) { xrtSetErrorKind(XERR_RANGE); return false; }
        return xl6ConcurrentDeadline(options.timeoutMs,deadline);
    }
    if (options.mode==XL6_CHANNEL_UNTIL_V2 && options.deadlineUs==UINT64_MAX) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    *deadline=options.mode==XL6_CHANNEL_WAIT_V1 ? XRT_DEADLINE_NEVER : options.deadlineUs;
    return true;
}
static inline bool xl6ChannelWaitStatus(xwaitresult native,bool sending,unsigned mode,XL6_ChannelStatusV1* result)
{
    switch (native) {
    case XWAIT_OK: *result=XL6_CHANNEL_OK_V1; return true;
    case XWAIT_CLOSED: *result=XL6_CHANNEL_CLOSED_V1; return true;
    case XWAIT_CANCELLED: *result=XL6_CHANNEL_CANCELLED_V1; return true;
    case XWAIT_TIMEOUT:
        *result=mode==XL6_CHANNEL_TRY_V1 ? (sending ? XL6_CHANNEL_FULL_V1 : XL6_CHANNEL_EMPTY_V1) :
            XL6_CHANNEL_TIMEOUT_V1; return true;
    default: return false;
    }
}
#endif
