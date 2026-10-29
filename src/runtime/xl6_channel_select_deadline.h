#ifndef XL6_CHANNEL_SELECT_DEADLINE_H
#define XL6_CHANNEL_SELECT_DEADLINE_H
#include "xl_clock.h"
#include "xl6_channel_future.h"
#include "xl6_deadline_service.h"
typedef struct XL6_ChannelSelectDeadlineRuntimeV1 {
    XL6_ChannelFutureFrameV1* (*PrepareFor)(XL6_ChannelCaseOwnerV1* const*,size_t,int64_t);
    XL6_ChannelFutureFrameV1* (*PrepareUntil)(XL6_ChannelCaseOwnerV1* const*,size_t,XL6_DeadlineV1);
    const xvalue* (*ResultHandle)(const XL6_ChannelFutureFrameV1*);
    xfuture* (*Start)(XL6_ContinuationExecutor*,XL6_DeadlineServiceV1*,XL6_ChannelFutureFrameV1*,
        const xvalue*,const char*,size_t,xcancel*);
    void (*Discard)(XL6_ChannelFutureFrameV1*);
    xrtownershipref (*Ownership)(const XL6_ChannelFutureFrameV1*);
} XL6_ChannelSelectDeadlineRuntimeV1;
XL6_CONTINUATION_API const XL6_ChannelSelectDeadlineRuntimeV1* xl6ChannelSelectDeadlineRuntimeV1Get(void);
#endif
