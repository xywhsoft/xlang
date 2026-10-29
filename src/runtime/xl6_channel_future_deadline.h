#ifndef XL6_CHANNEL_FUTURE_DEADLINE_H
#define XL6_CHANNEL_FUTURE_DEADLINE_H
#include "xl6_channel_future_core.h"
#include "xl6_deadline_service.h"
/* Private resident composition. Role is a permanent exact identity, not a
 * string/method-name dispatch. Attach is private and serialized; no service
 * reference is needed until Start. The caller owns service through Start.
 * Successful Arm owns a real frame reference plus an independent native
 * subscription reference until the source's physical Drop. The ticket's
 * actual service lease and both references remain visible to graph tracing. */
XL6_CONTINUATION_API bool xl6ChannelFutureDeadlineAttachV1(XL6_ChannelFutureFrameV1*,xdeadline,const void*);
XL6_CONTINUATION_API bool xl6ChannelFutureDeadlineMatchesV1(const XL6_ChannelFutureFrameV1*,const void*);
XL6_CONTINUATION_API xfuture* xl6ChannelFutureDeadlineStartV1(const void*,XL6_ContinuationExecutor*,
    XL6_DeadlineServiceV1*,XL6_ChannelFutureFrameV1*,const xvalue*,const char*,size_t,xcancel*);
#endif
