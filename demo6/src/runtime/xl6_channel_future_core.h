#ifndef XL6_CHANNEL_FUTURE_CORE_H
#define XL6_CHANNEL_FUTURE_CORE_H
#include "include/xruntime_decl.h"
#include "xl6_channel_future.h"
#include "xl6_future_payload.h"

/* Compiler-private resident provider contract. A provider is an immutable,
 * process-resident table; no language function, module callback or descriptor
 * storage is retained. Only its own exact Ops are admitted by its owning domain.
 * State is uniquely contained in ONE stable core allocation, aligned to max_align_t.
 * Notify/Empty/Trace are pure bookkeeping; Complete/Cleanup run only on workers
 * (or private preparation rollback), never from a notification/mechanical Drop.
 * Cleanup consumes every duty on either outcome and makes Empty true.
 * An accepted frame is never GC-cancelled; physical data pins don't gate delivery.
 * This table is not a certificate for arbitrary host/module callbacks. */
typedef struct XL6_ChannelFutureProviderV1 {
    size_t size,stateSize;
    XL6_FuturePayloadKind payloadKind;
    const xrtownershipops* ops;
    const xchannelsubscribeownershipv1* subscription;
    const xcancelwatchownershipv1* cancellation;
    void (*Notify)(void*,xchannelselectresult,void*);
    bool (*Complete)(void*,xchannelselectresult,int64_t*);
    void (*Cleanup)(void*);
    bool (*Trace)(const void*,xrtownershipvisitor,void*);
    bool (*Empty)(const void*);
} XL6_ChannelFutureProviderV1;
XL6_CONTINUATION_API XL6_ChannelFutureFrameV1* xl6ChannelFutureCoreCreateV1(const XL6_ChannelFutureProviderV1*);
XL6_CONTINUATION_API void* xl6ChannelFutureCoreStateV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API const XL6_ChannelFutureProviderV1* xl6ChannelFutureCoreProviderV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API bool xl6ChannelFutureCoreHandleTakeV1(XL6_ChannelFutureFrameV1*,xvalue**);
XL6_CONTINUATION_API bool xl6ChannelFutureCoreSubscribeV1(XL6_ChannelFutureFrameV1*,const xchannelsubscribecase*,size_t);
XL6_CONTINUATION_API const xvalue* xl6ChannelFutureCoreResultHandleV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API xfuture* xl6ChannelFutureCoreStartV1(XL6_ContinuationExecutor*,XL6_ChannelFutureFrameV1*,
    const xvalue*,const char*,size_t,xcancel*);
XL6_CONTINUATION_API void xl6ChannelFutureCoreDiscardV1(XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API xrtownershipref xl6ChannelFutureCoreOwnershipV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API bool xl6ChannelFutureCoreCountV1(const void*,const XL6_ChannelFutureProviderV1*,size_t*);
XL6_CONTINUATION_API bool xl6ChannelFutureCoreTraceV1(const void*,const XL6_ChannelFutureProviderV1*,xrtownershipvisitor,void*);
XL6_CONTINUATION_API const xrtownershipadapterv1* xl6ChannelFutureCoreAdapterV1(xrtownershipref,
    const XL6_ChannelFutureProviderV1*,const xrtownershippreparationv1**);
XL6_CONTINUATION_API void xl6ChannelFutureCoreNotifyV1(xchannelsubscription*,void*);
XL6_CONTINUATION_API void xl6ChannelFutureCoreDataDropV1(const void*);
XL6_CONTINUATION_API void xl6ChannelFutureCoreCancelV1(void*);
XL6_CONTINUATION_API void xl6ChannelFutureCoreCancelDropV1(const void*);
XL6_CONTINUATION_API const xfutureproducerownershipv1* xl6ChannelFutureCoreProducerPolicyV1Get(void);
XL6_CONTINUATION_API const XL6_ContinuationStorageV2* xl6ChannelFutureCoreStorageV2Get(void);
#endif
