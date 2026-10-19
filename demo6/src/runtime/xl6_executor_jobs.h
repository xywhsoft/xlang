#ifndef XL6_EXECUTOR_JOBS_H
#define XL6_EXECUTOR_JOBS_H
#include "xl6_closure.h"
struct xexecutor;
struct xexecutordataownershipv1;
struct xrtownershipquiescencev1;
struct xrtownershippreparationv1;
typedef struct XL6_ExecutorFailuresV1 XL6_ExecutorFailuresV1;
typedef struct XL6_ExecutorFailureSnapshotV1 {
    size_t size;
    uint64_t count;
    bool saturated;
} XL6_ExecutorFailureSnapshotV1;
typedef enum XL6_ExecutorWaitStatusV1 {
    XL6_EXECUTOR_READY_V1=0,
    XL6_EXECUTOR_TIMEOUT_V1=1
} XL6_ExecutorWaitStatusV1;

/* Explicit detached-work failure ledger. Each accepted work record owns one
 * actual reference, independent of the caller. First means the first failed
 * work to RECORD after body AND capture cleanup, not submission order. A body
 * plus cleanup failure counts once; ordinary cancellation alone does not count.
 * The first diagnostic may be NULL; that failure must never be replaced by a
 * later allocated error. count saturates with an explicit overflow indicator.
 * Inspect/Report never consume/reset the historical failure. */
XL6_CODE_LEASE_API XL6_ExecutorFailuresV1* xl6ExecutorFailuresCreateV1(void);
XL6_CODE_LEASE_API XL6_ExecutorFailuresV1* xl6ExecutorFailuresRetainV1(XL6_ExecutorFailuresV1*);
XL6_CODE_LEASE_API void xl6ExecutorFailuresReleaseV1(XL6_ExecutorFailuresV1*);
XL6_CODE_LEASE_API bool xl6ExecutorFailuresSnapshotV1(XL6_ExecutorFailuresV1*,XL6_ExecutorFailureSnapshotV1*);
/* Checked BOOL carrier, including failure without a diagnostic. Does not
 * propagate to an enclosing scope: the caller owns its ordinary error edge. */
XL6_CODE_LEASE_API bool xl6ExecutorThrowIfFailedV1(XL6_ExecutorFailuresV1*);

/* Compiler-private exact callback ABI: bool invoke(void* environment). The
 * compiler must verify a zero-argument void language closure before calling.
 * Retains real closures/code leases, never generated drop/trace descriptors.
 * Borrowed inputs survive preparation and rollback. Native batch acceptance is
 * all-or-nothing; failure runs no body, transfers no Data reference and retains
 * the caller's diagnostic independently of allocation. No arbitrary callback
 * or user destructor executes inside mutation/metadata locks.
 *
 * Native executor is BORROWED. Its owner must keep it valid until all accepted
 * work and cleanup tails finish, just as required by the XRT API. This producer
 * does NOT implement a managed Executor's final-owner retirement. */
XL6_CODE_LEASE_API bool xl6ExecutorSubmitClosureV1(struct xexecutor*,XL6_ExecutorFailuresV1*,XL6_Closure*);
XL6_CODE_LEASE_API bool xl6ExecutorSubmitClosuresV1(struct xexecutor*,XL6_ExecutorFailuresV1*,XL6_Closure* const*,size_t);
/* Close separately, as with native Executor. -1 infinite, 0 poll, positive
 * finite milliseconds. READY describes physical completion even if false
 * reports a recorded callback failure. Timeout does not report incomplete
 * work as success, nor consume an earlier callback error. */
XL6_CODE_LEASE_API bool xl6ExecutorWaitV1(struct xexecutor*,XL6_ExecutorFailuresV1*,int64_t,XL6_ExecutorWaitStatusV1*);

XL6_CODE_LEASE_API struct xrtownershipref xl6ExecutorFailuresOwnershipV1(const XL6_ExecutorFailuresV1*);
XL6_CODE_LEASE_API const struct xexecutordataownershipv1* xl6ExecutorClosureDataPolicyV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6ExecutorClosureQuiescenceV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ExecutorJobsOwnershipAdapterV1(
    struct xrtownershipref,const struct xrtownershippreparationv1**);
#endif
