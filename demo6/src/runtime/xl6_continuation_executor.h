#ifndef XL6_CONTINUATION_EXECUTOR_H
#define XL6_CONTINUATION_EXECUTOR_H
#include "xl6_continuation_queue.h"

typedef struct XL6_ContinuationExecutor XL6_ContinuationExecutor;
typedef enum XL6_ContinuationBlockingAdmission {
    XL6_CONTINUATION_BLOCKING_ADMITTED = 0,
    XL6_CONTINUATION_BLOCKING_LIMIT,
    XL6_CONTINUATION_BLOCKING_RESOURCE_FAILURE
} XL6_ContinuationBlockingAdmission;

/* Runtime execution domain, independent of Future producers. Limits bound
 * ticket admission and OS-thread high-water use, not language value types. */
XL6_ContinuationExecutor* xl6ContinuationExecutorCreate(
    size_t capacity, size_t initialWorkers, size_t maxWorkers);
/* General-language strategy: at least one provisioned worker per accepted
 * root activation. Root admission provisions before publication; resource
 * failure aborts the private reservation and releases its code pin. Accepted
 * run/release/host-drop callbacks may block WITHOUT fallible call-site guards.
 * Acyclic waits on other accepted activations cannot starve for workers.
 * This is bounded admission, not a promise to resolve program-level cycles or
 * foreign operations that never finish. Workers are pooled up to capacity and
 * retained at their high-water count until shutdown. Admission may therefore
 * cost an OS thread; callers choose capacity as an explicit resource policy. */
XL6_CONTINUATION_API XL6_ContinuationExecutor* xl6ContinuationExecutorCreateGuaranteed(
    size_t capacity, size_t initialWorkers);
XL6_ContinuationAdmission xl6ContinuationExecutorReserve(
    XL6_ContinuationExecutor*, XL6_ContinuationTicket*,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data);
XL6_CONTINUATION_API XL6_ContinuationAdmission xl6ContinuationExecutorReserveOwned(
    XL6_ContinuationExecutor*, XL6_ContinuationTicket*,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data, const XL6_CodeOwner*);
/* In the compensating strategy (Create), may-block calls must acquire before entering the
 * foreign call. The last runnable worker provisions a replacement first;
 * failure does not enter the blocking region. Nesting counts once. Calls on
 * unrelated threads are admitted without changing this executor's capacity.
 * Every admitted enter must have a matching leave on the same native thread.
 * In the guaranteed strategy these guards are optional balanced bookkeeping:
 * no worker is created and no call-site admission failure is introduced. */
XL6_ContinuationBlockingAdmission xl6ContinuationBlockingEnter(XL6_ContinuationExecutor*);
void xl6ContinuationBlockingLeave(XL6_ContinuationExecutor*);
/* Close refuses NEW reservations; already reserved Future outcomes still run.
 * This is not cancellation. Destroy is a non-destructive busy check followed
 * by worker join, and never succeeds while run/release/pending leases exist.
 * External lifecycle serialization is required for Create/Close/Destroy. */
XL6_CONTINUATION_API void xl6ContinuationExecutorClose(XL6_ContinuationExecutor*);
/* Snapshot of accepted work, including physical release/code-owner tails.
 * Does not close admission or stop workers. Callers must serialize producers
 * before using an idle observation as a drain boundary; it is not by itself
 * permission to destroy an executor or unload a module. NULL is idle. */
XL6_CONTINUATION_API bool xl6ContinuationExecutorIsIdle(XL6_ContinuationExecutor*);
XL6_CONTINUATION_API bool xl6ContinuationExecutorDestroy(XL6_ContinuationExecutor*);
/* Frozen owning view of the actual executor allocation. Create supplies one
 * owner reference; successful Destroy consumes that slot after joining the
 * workers. Plan/native holds keep the same terminal allocation alive. Each
 * worker's uniquely contained wait slot traces its own real queue reference
 * only while parked. Starting, executing, blocking, native entry, joining
 * and retirement phases refuse inspection. No OS thread is detached.
 * Queue and child frame policies require independent admission. Preparation
 * needs that same plan's prepared queue and waits nonblockingly for worker
 * exit/join; it does not cancel tasks or infer module lifetime. Lifecycle
 * callers must serialize Close/Destroy with plan preparation/finish. */
XL6_CONTINUATION_API struct xrtownershipref xl6ContinuationExecutorOwnership(const XL6_ContinuationExecutor*);
XL6_CONTINUATION_API const struct xrtownershipadapterv1* xl6ContinuationExecutorOwnershipAdapterV1(
    struct xrtownershipref,const struct xrtownershippreparationv1**);
#endif
