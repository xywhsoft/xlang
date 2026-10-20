#ifndef XL6_FUTURE_CALLBACK_H
#define XL6_FUTURE_CALLBACK_H
#include <stddef.h>
#include "xl6_code_lease.h"
#include "xl6_capture.h"
#include "xl6_capture_v2.h"
struct xfuture;
struct xpromise;
struct xcancel;
struct xfutureresult;
struct xrtownershipref;
typedef void (*XL6_FutureCallbackProc)(const struct xfutureresult*, struct xpromise*, struct xcancel*, void*);
typedef void (*XL6_FutureCallbackDrop)(void*, void*);

/* A resident activation for the verified Future callback capture program.
 * Language invocation/cleanup/result decisions remain in the IR producer.
 * All physical ownership and callback entry/return tails remain resident. */
typedef struct XL6_FutureCallbackRuntimeV1 {
    /* One allocation/graph node, aligned producer data. Registration inherits
     * this thread's accepted scope for the exact same owner; otherwise it needs
     * new admission. This permits child continuations after Close without
     * reopening root admission or borrowing another thread's capability.
     * Caller independently pins its entering code through failure rollback.
     * Initialize owning slots before fallible capture. Drop only cleans data;
     * it must not free it. Trace describes precisely those capture owners. */
    void* (*Allocate)(const XL6_CodeOwner*, size_t, XL6_FutureCallbackProc,
        XL6_FutureCallbackDrop, XL6_CodeContextTrace);
    void (*Discard)(void*);
    struct xrtownershipref (*Ownership)(const void*);
    /* NULL preserves the private initialized capture frame for Discard.
     * Success consumes it, including READY/synchronous callbacks. A separate
     * real source reference protects borrowed input payload through invocation,
     * even after the producer detaches/cleans its captured source. The promise,
     * cancellation token and Watch registration are owned by this same node.
     * Callback code and every result destructor/trace have independent lifetime
     * contracts. A callback must decide/publish after capture cleanup; this
     * runtime does not guess then/catch/finally semantics or clone payloads.
     * Private/publishing/active/retiring nodes refuse Count/Trace. Neither user
     * callbacks nor waits run under an ownership mutation scope or frame lock. */
    struct xfuture* (*Start)(void*, struct xfuture* source);
} XL6_FutureCallbackRuntimeV1;
XL6_CODE_LEASE_API const XL6_FutureCallbackRuntimeV1* xl6FutureCallbackRuntimeV1Get(void);
/* Additive V2: the original four-slot table is unchanged. A verified physical
 * layout replaces generated Drop/Trace. Start/Discard/Ownership keep the same
 * physical node and V1 transport entry points; no callback gains graph
 * collection eligibility merely by having a layout. */
typedef struct XL6_FutureCallbackRuntimeV2 {
    void* (*Allocate)(const XL6_CodeOwner*, size_t, XL6_FutureCallbackProc, const XL6_CaptureLayoutV1*);
    void (*Cleanup)(void*);
} XL6_FutureCallbackRuntimeV2;
XL6_CODE_LEASE_API const XL6_FutureCallbackRuntimeV2* xl6FutureCallbackRuntimeV2Get(void);
/* Additive V3: the producer explicitly owns the source's entire production
 * chain. Output cancellation requests source cancellation, without removing
 * the accepted source callback or changing its terminal-outcome semantics.
 * This is not valid for a source shared with unrelated consumers. The same
 * V1/V2 allocation, rollback, code scope and capture contracts apply. The
 * resident observer owns one real frame reference and is unregistered before
 * transport retirement; it never calls generated code without that frame. */
typedef struct XL6_FutureCallbackRuntimeV3 {
    struct xfuture* (*StartExclusive)(void*, struct xfuture* source);
} XL6_FutureCallbackRuntimeV3;
XL6_CODE_LEASE_API const XL6_FutureCallbackRuntimeV3* xl6FutureCallbackRuntimeV3Get(void);
/* Exact V2 capture payload, sharing the V1 Start/Discard/Ownership and V3
 * exclusive-source protocol. No new Watch, producer or scheduler identity. */
typedef struct XL6_FutureCallbackRuntimeV4 {
    void* (*Allocate)(const XL6_CodeOwner*,size_t,XL6_FutureCallbackProc,const XL6_CaptureLayoutV2*);
    void (*Cleanup)(void*);
} XL6_FutureCallbackRuntimeV4;
XL6_CODE_LEASE_API const XL6_FutureCallbackRuntimeV4* xl6FutureCallbackRuntimeV4Get(void);
struct XL6_ContinuationExecutor;
struct XL6_ContinuationStorageV2;
struct xl6_foreign_error_result;
struct xerror;
/* Shared terminal failure normalization; no ready-queue dependency. Consumes
 * the diagnostic and preserves the caller's ambient diagnostic independently. */
XL6_CODE_LEASE_API void xl6FutureCallbackFailV1(struct xpromise*,struct xerror*);
/* Explicit ready-queue language contract. Allocate owns the same closed V2
 * capture layout, code pin and physical frame as V4. Start reserves execution
 * BEFORE registering its Watch; NULL leaves a private frame for V1 Discard.
 * Success transfers that reference to the fixed executor. Source completion
 * only signals preallocated events; language invocation AND cleanup run on a
 * worker, including already-ready sources. Cleanup is called exactly once by
 * that active callback, after its last language invocation and BEFORE publishing
 * output: it cleans captures and commits logical completion. Private rollback
 * and physical fallback use V1 Discard/shared cleanup instead, not this barrier.
 * Close rejects NEW callback roots;
 * already accepted callbacks finish normally. No blocking task/stack per wait.
 * V1-4 remain explicit inline native transports, not this language contract. */
typedef struct XL6_FutureCallbackRuntimeV5 {
    void* (*Allocate)(const XL6_CodeOwner*,size_t,XL6_FutureCallbackProc,const XL6_CaptureLayoutV2*);
    /* Consumes incoming.error; preserves the FIRST failure, including a false
     * status with no diagnostic. Returns an owned diagnostic, never an ambient
     * error-pointer success inference. */
    struct xl6_foreign_error_result (*Cleanup)(void*,struct xl6_foreign_error_result);
    struct xfuture* (*Start)(struct XL6_ContinuationExecutor*,void*,struct xfuture*);
    /* Consumes the owned diagnostic on every path. Diagnostic-free failure is
     * normalized to static INTERNAL only at this native Future boundary. */
    void (*Fail)(struct xpromise*,struct xerror*);
} XL6_FutureCallbackRuntimeV5;
XL6_CODE_LEASE_API const XL6_FutureCallbackRuntimeV5* xl6FutureCallbackRuntimeV5Get(void);
XL6_CODE_LEASE_API const struct xfuturewatchownershipv1* xl6FutureCallbackQueuedWatchPolicyV1Get(void);
XL6_CODE_LEASE_API const struct XL6_ContinuationStorageV2* xl6FutureCallbackQueuedStorageV2Get(void);
/* Immutable identity for the real output-Future -> producer-frame reference.
 * Recognition of this policy alone does not admit the frame, Watch or captures
 * to collection; every physical child still requires its own full protocol. */
struct xfutureproducerownershipv1;
XL6_CODE_LEASE_API const struct xfutureproducerownershipv1* xl6FutureCallbackProducerPolicyV1Get(void);
struct xfuturewatchownershipv1;
struct xrtownershipadapterv1;
struct xrtownershippreparationv1;
XL6_CODE_LEASE_API const struct xfuturewatchownershipv1* xl6FutureCallbackWatchPolicyV1Get(void);
struct xcancelwatchownershipv1;
XL6_CODE_LEASE_API const struct xcancelwatchownershipv1* xl6FutureCallbackCancelPolicyV1Get(void);
/* Only immutable validated V2 layouts. Both descriptors are mandatory: the
 * accepted callback must run before captures or code can be retired. */
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6FutureCallbackOwnershipAdapterV1(
    struct xrtownershipref, const struct xrtownershippreparationv1**);
#endif
