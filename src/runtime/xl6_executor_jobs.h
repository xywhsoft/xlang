#ifndef XL6_EXECUTOR_JOBS_H
#define XL6_EXECUTOR_JOBS_H
#include "xl6_closure.h"
#include "xl6_execution_failures.h"
struct xexecutor;
struct xexecutordataownershipv1;
struct xexecutorworkerexitownershipv1;
struct xexecutorowneditemv1;
struct xrtownershipquiescencev1;
struct xrtownershippreparationv1;
typedef enum XL6_ExecutorWaitStatusV1 {
    XL6_EXECUTOR_READY_V1=0,
    XL6_EXECUTOR_TIMEOUT_V1=1
} XL6_ExecutorWaitStatusV1;


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
XL6_CODE_LEASE_API bool xl6ExecutorSubmitClosureV1(struct xexecutor*,XL6_ExecutionFailuresV1*,XL6_Closure*);
XL6_CODE_LEASE_API bool xl6ExecutorSubmitClosuresV1(struct xexecutor*,XL6_ExecutionFailuresV1*,XL6_Closure* const*,size_t);
/* Compiler-private prepared owned records, not language Closure* storage.
 * The caller verifies a nonnullable array<func()> exact signature and pins
 * the source while Prepare reads its xvalue* slots. Prepare never starts work.
 * End that source pin BEFORE Commit: callbacks must not inherit an Array pin.
 * Commit performs ONE native atomic batch admission and consumes every record
 * on both acceptance and rejection. Discard also consumes a valid batch.
 * No semantic cleanup remains after successful admission. Empty is a no-op;
 * receiver/open admission is the caller's responsibility, before Prepare.
 * The stack record cannot be copied while it owns items. False may have no
 * allocated diagnostic; no rejected job body or failure-count notification. */
typedef struct XL6_ExecutorPreparedBatchV1 {
    struct xexecutorowneditemv1* items;
    size_t count;
} XL6_ExecutorPreparedBatchV1;
XL6_CODE_LEASE_API bool xl6ExecutorPrepareBoxedBatchV1(XL6_ExecutionFailuresV1*,
    struct xvalue* const*,size_t,uint64_t,XL6_ExecutorPreparedBatchV1*);
XL6_CODE_LEASE_API bool xl6ExecutorCommitBoxedBatchV1(struct xexecutor*,XL6_ExecutorPreparedBatchV1*);
XL6_CODE_LEASE_API bool xl6ExecutorDiscardBoxedBatchV1(XL6_ExecutorPreparedBatchV1*);
/* Close separately, as with native Executor. -1 infinite, 0 poll, positive
 * finite milliseconds. READY describes physical completion even if false
 * reports a recorded callback failure. Timeout does not report incomplete
 * work as success, nor consume an earlier callback error. */
XL6_CODE_LEASE_API bool xl6ExecutorWaitV1(struct xexecutor*,XL6_ExecutionFailuresV1*,int64_t,XL6_ExecutorWaitStatusV1*);

XL6_CODE_LEASE_API const struct xexecutordataownershipv1* xl6ExecutorClosureDataPolicyV1Get(void);
/* Native-owned ledger reference. Exit runs once per actual worker, not job;
 * checked ThreadLocal cleanup failures use the same first-error ledger. */
XL6_CODE_LEASE_API const struct xexecutorworkerexitownershipv1* xl6ExecutorWorkerExitPolicyV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6ExecutorClosureQuiescenceV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ExecutorJobsOwnershipAdapterV1(
    struct xrtownershipref,const struct xrtownershippreparationv1**);
#endif
