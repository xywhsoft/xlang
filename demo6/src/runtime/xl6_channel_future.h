#ifndef XL6_CHANNEL_FUTURE_H
#define XL6_CHANNEL_FUTURE_H
#include "xl6_channel_case_abi.h"
#include "xl6_continuation_executor.h"
struct xfuture;
struct xrtownershipref;
struct xrtownershipadapterv1;
struct xrtownershippreparationv1;
struct xchannelsubscribeownershipv1;
struct xfutureproducerownershipv1;
struct xcancelwatchownershipv1;
struct XL6_NativeHandlePolicyV1;
typedef struct XL6_ChannelFutureFrameV1 XL6_ChannelFutureFrameV1;

/* Resident-only unbounded subscription producer, not the public language
 * cutover. Prepare copies every case/message and preallocates its selection.
 * The borrowed handle is available ONLY while the builder remains private.
 * Start borrows an already-built result owning that EXACT handle: either the
 * handle itself (native carrier) or its exact object field. Field metadata is
 * used only during this call, never retained. Language lowering must seal the
 * nominal result/field contract; no native-leaf profile or second mapped Future
 * is involved. All allocations and admission precede native Start.
 * Success transfers the builder to the queue. NULL leaves it for Discard ONLY.
 * No producer notification or mechanical Drop executes semantic cleanup.
 * Logical handoff follows physical case unlink and the notification's final
 * operation access. Native Data pins preserve its remaining mechanical tail;
 * inspection pins prolong storage, never logical Future completion.
 * Channel cancellation is cooperative, preserving native ready-before-stop
 * arbitration; a cancelled selection is a normal owned result with status.
 * No finite deadline is implied: the generic timer source is a separate step. */
typedef struct XL6_ChannelFutureRuntimeV1 {
    XL6_ChannelFutureFrameV1* (*Prepare)(XL6_ChannelCaseOwnerV1* const*,size_t);
    const struct xvalue* (*ResultHandle)(const XL6_ChannelFutureFrameV1*);
    struct xfuture* (*Start)(XL6_ContinuationExecutor*,XL6_ChannelFutureFrameV1*,
        const struct xvalue*,const char*,size_t,struct xcancel*);
    void (*Discard)(XL6_ChannelFutureFrameV1*);
    struct xrtownershipref (*Ownership)(const XL6_ChannelFutureFrameV1*);
} XL6_ChannelFutureRuntimeV1;
XL6_CONTINUATION_API const XL6_ChannelFutureRuntimeV1* xl6ChannelFutureRuntimeV1Get(void);
/* Compiler-private single-owner builder. StartRetained adds the queue's own
 * reference BEFORE publication, leaving the builder's handle valid for its
 * ordinary lexical destructor. Calls on the private builder are serialized. */
XL6_CONTINUATION_API void xl6ChannelFutureBuilderDestroyV1(void*);
XL6_CONTINUATION_API struct xvalue* xl6ChannelFutureBuilderResultV1(void*);
XL6_CONTINUATION_API struct xfuture* xl6ChannelFutureBuilderStartV1(
    XL6_ContinuationExecutor*,void*,const struct xvalue*,const char*,size_t,struct xcancel*);
XL6_CONTINUATION_API const struct XL6_NativeHandlePolicyV1* xl6ChannelFutureBuilderHandlePolicyV1Get(void);
XL6_CONTINUATION_API const struct xchannelsubscribeownershipv1* xl6ChannelFutureSubscriptionPolicyV1Get(void);
XL6_CONTINUATION_API const struct xfutureproducerownershipv1* xl6ChannelFutureProducerPolicyV1Get(void);
XL6_CONTINUATION_API const struct xcancelwatchownershipv1* xl6ChannelFutureCancelPolicyV1Get(void);
XL6_CONTINUATION_API const XL6_ContinuationStorageV2* xl6ChannelFutureStorageV2Get(void);
XL6_CONTINUATION_API const struct xrtownershipadapterv1* xl6ChannelFutureOwnershipAdapterV1(
    struct xrtownershipref,const struct xrtownershippreparationv1**);
#endif
