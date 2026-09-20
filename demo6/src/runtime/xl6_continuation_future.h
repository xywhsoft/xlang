#ifndef XL6_CONTINUATION_FUTURE_H
#define XL6_CONTINUATION_FUTURE_H
#include "xl6_continuation_queue.h"
#include "xl6_capture.h"
typedef struct xfuture xfuture;
typedef union xfuturewatch xfuturewatch;

/* Register an uncommitted, reserved ticket using caller-owned Watch storage.
 * The caller retains a strong source lease through result capture. Success
 * transfers Watch notification ownership, but not frame/argument ownership:
 * the caller must Commit AFTER its handoff transaction is complete. READY
 * synthesizes both transport events; PENDING uses the real Watch callbacks.
 * Neither path executes language callbacks or allocates. After failure no
 * event has been delivered on a fresh ticket; the caller must Abort and roll
 * back its frame. Invalid reuse leaves a preexisting registration intact.
 * An accepted watch must not be detached or reused by its caller. Only the
 * produced-frame V4 CancelWait protocol below may detach it while preserving
 * queue execution ownership; cancellation is never permission to drop a job. */
/* The resident certified Watch (or legacy traced Watch) reports exactly one acquired storage lease. Its
 * release callback returns that lease before queue publication. It does not
 * trace frame slots directly or cancel the independent execution lease. */
XL6_CONTINUATION_API bool xl6ContinuationWatchRegister(XL6_ContinuationTicket*, xfuturewatch*, xfuture*);
/* The generated frame and its embedded ticket/Watch form ONE physical node.
 * The closed descriptor covers their exact, nonoverlapping byte locations.
 * The sparse payload describes only language captures, not transport fields.
 * Descriptors stay immutable and resident through the last actual frame Drop. */
typedef struct XL6_ContinuationFrameLayoutV2 {
    size_t size;
    const XL6_ActivationCaptureLayoutV1* captures;
    size_t ticket, watch, source, outer;
} XL6_ContinuationFrameLayoutV2;
typedef struct XL6_ContinuationFrameRuntimeV2 {
    /* Private allocation has one reference. Producer initializes all capture
     * slots/flags before fallible acquisition. Runtime initializes transport.
     * Bind after Reserve, before Watch; it retains the accepted code lease.
     * Commit transfers the caller's one reference to the queue. Failure keeps
     * it with the caller, who Aborts any reservation and then Discards. */
    void* (*Allocate)(size_t, const XL6_ContinuationFrameLayoutV2*);
    bool (*Bind)(void*);
    /* Cleanup is the lexical capture barrier, before publishing the outcome.
     * Retire additionally detaches source/output and marks physical storage
     * ready, but does NOT consume a reference. Only the queue calls Retire for
     * committed frames. Discard performs Retire + Release for private frames.
     * All user cleanup runs outside mutation and under a real code scope. */
    struct xl6_foreign_error_result (*Cleanup)(void*, struct xl6_foreign_error_result);
    XL6_ContinuationProc Retire;
    void (*Discard)(void*);
    void* (*Retain)(void*);
    void (*Release)(void*);
    struct xrtownershipref (*Ownership)(const void*);
} XL6_ContinuationFrameRuntimeV2;
XL6_CONTINUATION_API const XL6_ContinuationFrameRuntimeV2* xl6ContinuationFrameRuntimeV2Get(void);
/* Explicit produced-activation binding, additive to V2 physical storage.
 * Root requires a fresh private outer Promise/Future pair, including a retry
 * after Abort: the old pair's bind-once producer association is not replaced.
 * A successor must carry that same outer Promise identity. The stable producer
 * owns the current frame and each bound frame owns a producer reference.
 * A successor borrows no lifetime: binding retains the same producer; Commit
 * transfers its actual current-frame edge; Abort only undoes that provisional
 * binding. No chain of old frames is kept. Normal outcome publication returns
 * the output's producer reference; normal Retire detaches the remaining edges.
 * Allocation/Bind failure leaves the caller's reservation/frame intact. */
typedef struct XL6_ContinuationFrameRuntimeV3 {
    bool (*Bind)(void*);
} XL6_ContinuationFrameRuntimeV3;
XL6_CONTINUATION_API const XL6_ContinuationFrameRuntimeV3* xl6ContinuationFrameRuntimeV3Get(void);
struct xfutureresult;
/* Once-only await cancellation belongs to the stable logical producer, not
 * copied language frame fields. Observe is called by the queue's run callback.
 * CancelWait is an explicit owning-host operation: it does not infer that a
 * graph is unreachable. The caller must own a real frame/entry reference.
 * It requests output cancellation, detaches ONLY this frame's source Watch,
 * and hands the accepted activation back to the queue's normal resume CFG.
 * It never closes the source, skips finally, or consumes the execution credit.
 * An already-ready/running frame or cancellation already observed returns false
 * without interrupting later awaits (including awaits in catch/finally).
 * Source completion and cancellation arbitrate exactly one Ready event; this
 * call may wait for a source Watch callback, without a mutation/queue/frame lock.
 * The owning host must call outside any enclosing ownership scope or callback
 * that holds one; this operation cannot suspend an enclosing caller's scope.
 * This is not authorization to collect a pending dependency SCC. */
typedef struct XL6_ContinuationFrameRuntimeV4 {
    bool (*Observe)(void*, struct xfutureresult*);
    bool (*CancelWait)(void*);
} XL6_ContinuationFrameRuntimeV4;
XL6_CONTINUATION_API const XL6_ContinuationFrameRuntimeV4* xl6ContinuationFrameRuntimeV4Get(void);
struct xfutureproducerownershipv1;
struct xfuturewatchownershipv1;
XL6_CONTINUATION_API const struct xfutureproducerownershipv1* xl6ContinuationProducerPolicyV1Get(void);
XL6_CONTINUATION_API const struct xfuturewatchownershipv1* xl6ContinuationWatchPolicyV1Get(void);
/* Exact table identity for a queue graph resolver's allow-list. This does not
 * admit arbitrary storage or replace per-frame/producer/Watch admission. */
XL6_CONTINUATION_API const XL6_ContinuationStorageV2* xl6ContinuationProducedStorageV2Get(void);
struct xrtownershipadapterv1;
struct xrtownershippreparationv1;
/* Exact resident frame and stable producer identities only. A pending activation is not
 * prepared; do not remove its Watch or skip its source finally to collect it.
 * Native/collector references keep the actual allocation and code alive, not
 * permit concurrent access to language slots. All children need admission. */
XL6_CONTINUATION_API const struct xrtownershipadapterv1* xl6ContinuationFrameOwnershipAdapterV1(
    struct xrtownershipref, const struct xrtownershippreparationv1**);
#endif
