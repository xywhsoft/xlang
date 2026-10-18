#ifndef XL6_CHANNEL_DIRECT_FUTURE_H
#define XL6_CHANNEL_DIRECT_FUTURE_H
#include "xl6_channel_future.h"
#include "xl6_sequence_abi.h"
#include "xl6_future_value.h"
typedef struct XL6_ChannelReceiveResultV1 XL6_ChannelReceiveResultV1;
/* Native prerequisite, not a public language method declaration. Prepare
 * copies a send value ONCE or creates one private empty receive record. The
 * caller keeps any prepublication receive carrier private until completion.
 * Start success transfers the builder to the queue; failure leaves it for
 * Discard only. Send publishes owned int64 status; receive publishes the
 * exact prebuilt owned record/class. Neither path creates a public Case. */
typedef struct XL6_ChannelDirectFutureRuntimeV1 {
    XL6_ChannelFutureFrameV1* (*PrepareSend)(XL6_SequenceLayoutV1,const xvalue*,const void*);
    XL6_ChannelFutureFrameV1* (*PrepareReceive)(XL6_SequenceLayoutV1,const xvalue*);
    const xvalue* (*ResultHandle)(const XL6_ChannelFutureFrameV1*);
    struct xfuture* (*StartSend)(XL6_ContinuationExecutor*,XL6_ChannelFutureFrameV1*,struct xcancel*);
    struct xfuture* (*StartReceive)(XL6_ContinuationExecutor*,XL6_ChannelFutureFrameV1*,
        const xvalue*,const char*,size_t,struct xcancel*);
    void (*Discard)(XL6_ChannelFutureFrameV1*);
    struct xrtownershipref (*Ownership)(const XL6_ChannelFutureFrameV1*);
} XL6_ChannelDirectFutureRuntimeV1;
XL6_CONTINUATION_API const XL6_ChannelDirectFutureRuntimeV1* xl6ChannelDirectFutureRuntimeV1Get(void);
XL6_CONTINUATION_API bool xl6ChannelReceiveResultInfoV1(const XL6_ChannelReceiveResultV1*,int64_t*);
/* Copies exact T to a private initially empty output. False (even without a
 * diagnostic) leaves output unchanged. Non-OK or unpublished records refuse
 * value access; OK nullable null is still a value. Profile is never retained. */
XL6_CONTINUATION_API bool xl6ChannelReceiveResultReadV1(XL6_FutureValueProfileV1,
    const XL6_ChannelReceiveResultV1*,void*);
XL6_CONTINUATION_API void xl6ChannelReceiveResultDestroyV1(XL6_ChannelReceiveResultV1*);
XL6_CONTINUATION_API const struct XL6_NativeHandlePolicyV1* xl6ChannelReceiveResultHandlePolicyV1Get(void);
XL6_CONTINUATION_API const struct xchannelsubscribeownershipv1* xl6ChannelDirectFutureSubscriptionPolicyV1Get(void);
XL6_CONTINUATION_API const struct xcancelwatchownershipv1* xl6ChannelDirectFutureCancelPolicyV1Get(void);
XL6_CONTINUATION_API const struct xrtownershipquiescencev1* xl6ChannelReceiveResultQuiescenceV1Get(void);
XL6_CONTINUATION_API const struct xrtownershipadapterv1* xl6ChannelDirectFutureOwnershipAdapterV1(
    struct xrtownershipref,const struct xrtownershippreparationv1**);
#endif
