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
    /* A non-NULL token is ONLY the claimed, unstarted builder's rollback.
     * Accepted worker completion always passes NULL; it cannot operate on a
     * claimed channel. Rollback may release edges claimed by the SAME plan. */
    void (*Cleanup)(void*,const void*);
    bool (*Trace)(const void*,xrtownershipvisitor,void*);
    bool (*Empty)(const void*);
} XL6_ChannelFutureProviderV1;
/* Optional uniquely-contained resident interruption transport, not a language
 * callback or native-leaf certificate. Infinite producers retain no instance
 * and introduce no timer dependency. Arm runs after admission/before native
 * Start. Detach never waits for a physical notification tail. RecordFailure
 * is mechanical and runs under the core gate, only before native Notify.
 * TakeFailure receives the immutable native arbitration result: an auxiliary
 * stop cannot retroactively replace a ready commit or an earlier stop.
 * TakeFailure/Cleanup run on the worker or private rollback, consume every
 * semantic duty and preserve rich false/NULL. Drop is mechanical allocation
 * release only; the independent transport owner keeps all storage alive. */
typedef struct XL6_ChannelFutureWakeV1 {
    size_t size;
    bool (*Arm)(void*,const void*);
    void (*Detach)(void*);
    void (*RecordFailure)(void*,const xerror*);
    bool (*TakeFailure)(void*,xchannelselectresult);
    void (*Cleanup)(void*);
    bool (*Trace)(const void*,xrtownershipvisitor,void*);
    bool (*Empty)(const void*);
    void (*Drop)(void*);
} XL6_ChannelFutureWakeV1;
XL6_CONTINUATION_API XL6_ChannelFutureFrameV1* xl6ChannelFutureCoreCreateV1(const XL6_ChannelFutureProviderV1*);
/* Publish a fully prepared, still private builder to a managed handle. Raw
 * construction remains uninspectable. The caller serializes preparation and
 * Start; a claimed private builder cannot Start. Its ownership preparation
 * may perform the same private rollback outside Freeze, without consuming
 * the builder's creator credit. Accepted work is NEVER rolled back by GC. */
XL6_CONTINUATION_API bool xl6ChannelFutureCorePublishBuilderV1(XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API void* xl6ChannelFutureCoreStateV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API const XL6_ChannelFutureProviderV1* xl6ChannelFutureCoreProviderV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API bool xl6ChannelFutureCoreHandleTakeV1(XL6_ChannelFutureFrameV1*,xvalue**);
XL6_CONTINUATION_API bool xl6ChannelFutureCoreSubscribeV1(XL6_ChannelFutureFrameV1*,const xchannelsubscribecase*,size_t);
XL6_CONTINUATION_API const xvalue* xl6ChannelFutureCoreResultHandleV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API xfuture* xl6ChannelFutureCoreStartV1(XL6_ContinuationExecutor*,XL6_ChannelFutureFrameV1*,
    const xvalue*,const char*,size_t,xcancel*);
XL6_CONTINUATION_API bool xl6ChannelFutureCoreWakeTakeV1(XL6_ChannelFutureFrameV1*,
    const XL6_ChannelFutureWakeV1*,void**);
XL6_CONTINUATION_API const XL6_ChannelFutureWakeV1* xl6ChannelFutureCoreWakePolicyV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API void* xl6ChannelFutureCoreWakeDataV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API xfuture* xl6ChannelFutureCoreStartWakeV1(XL6_ContinuationExecutor*,XL6_ChannelFutureFrameV1*,
    const xvalue*,const char*,size_t,xcancel*,const void*);
/* Preparation takes an actual reference before publication; notification
 * borrows that independently owned reference, never fallibly retains then.
 * Caller must trace/drop it and keep both frame/subscription alive through
 * Stop. A source failure is recorded only while the native result is pending. */
XL6_CONTINUATION_API bool xl6ChannelFutureCoreRetainV1(XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API xchannelsubscription* xl6ChannelFutureCoreSubscriptionRefV1(XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API void xl6ChannelFutureCoreStopV1(XL6_ChannelFutureFrameV1*,xchannelsubscription*,
    xwaitresult,bool,const xerror*);
XL6_CONTINUATION_API void xl6ChannelFutureCoreDiscardV1(XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API xrtownershipref xl6ChannelFutureCoreOwnershipV1(const XL6_ChannelFutureFrameV1*);
XL6_CONTINUATION_API bool xl6ChannelFutureCoreCountV1(const void*,const XL6_ChannelFutureProviderV1*,size_t*);
/* Frozen, read-only transient classification, NOT Count/Trace admission. */
XL6_CONTINUATION_API bool xl6ChannelFutureCoreBusyV1(const void*,const XL6_ChannelFutureProviderV1*);
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
