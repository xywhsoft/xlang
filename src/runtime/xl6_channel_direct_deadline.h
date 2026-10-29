#ifndef XL6_CHANNEL_DIRECT_DEADLINE_H
#define XL6_CHANNEL_DIRECT_DEADLINE_H
#include "xl_clock.h"
#include "xl6_channel_direct_future.h"
#include "xl6_deadline_service.h"
/* Native prerequisite for finite typed methods. For takes nonnegative ms;
 * Until takes absolute monotonic milliseconds, excluding NEVER. Fix the absolute budget
 * BEFORE message copy/result construction. Start transfers the same Future
 * producer to fixed workers; failures leave a private Discard obligation. */
typedef struct XL6_ChannelDirectDeadlineRuntimeV1 {
    XL6_ChannelFutureFrameV1* (*PrepareSendFor)(XL6_SequenceLayoutV1,const xvalue*,const void*,int64_t);
    XL6_ChannelFutureFrameV1* (*PrepareSendUntil)(XL6_SequenceLayoutV1,const xvalue*,const void*,XL6_DeadlineV1);
    XL6_ChannelFutureFrameV1* (*PrepareReceiveFor)(XL6_SequenceLayoutV1,const xvalue*,int64_t);
    XL6_ChannelFutureFrameV1* (*PrepareReceiveUntil)(XL6_SequenceLayoutV1,const xvalue*,XL6_DeadlineV1);
    const xvalue* (*ResultHandle)(const XL6_ChannelFutureFrameV1*);
    xfuture* (*StartSend)(XL6_ContinuationExecutor*,XL6_DeadlineServiceV1*,XL6_ChannelFutureFrameV1*,xcancel*);
    xfuture* (*StartReceive)(XL6_ContinuationExecutor*,XL6_DeadlineServiceV1*,XL6_ChannelFutureFrameV1*,
        const xvalue*,const char*,size_t,xcancel*);
    void (*Discard)(XL6_ChannelFutureFrameV1*);
    xrtownershipref (*Ownership)(const XL6_ChannelFutureFrameV1*);
} XL6_ChannelDirectDeadlineRuntimeV1;
XL6_CONTINUATION_API const XL6_ChannelDirectDeadlineRuntimeV1* xl6ChannelDirectDeadlineRuntimeV1Get(void);
#endif
