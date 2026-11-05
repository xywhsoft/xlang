#ifndef XL6_EXECUTION_FAILURES_H
#define XL6_EXECUTION_FAILURES_H
#include "xl6_code_lease.h"
struct xerror;
struct xrtownershipops;
struct xrtownershipref;
struct xrtownershipadapterv1;

typedef struct XL6_ExecutionFailuresV1 XL6_ExecutionFailuresV1;
typedef struct XL6_ExecutionFailureSnapshotV1 {
    size_t size;
    uint64_t count;
    bool saturated;
} XL6_ExecutionFailureSnapshotV1;

/* A shared execution failure ledger, not a scheduler. First means the first
 * failed transaction to record, including a failure with NULL diagnostic.
 * Record consumes its diagnostic on BOTH success and failure. History is
 * stable: explicit Check observes notification, never resets error/count.
 * A later record re-arms notification even after the counter saturates. */
XL6_CODE_LEASE_API XL6_ExecutionFailuresV1* xl6ExecutionFailuresCreateV1(void);
XL6_CODE_LEASE_API XL6_ExecutionFailuresV1* xl6ExecutionFailuresRetainV1(XL6_ExecutionFailuresV1*);
XL6_CODE_LEASE_API void xl6ExecutionFailuresReleaseV1(XL6_ExecutionFailuresV1*);
XL6_CODE_LEASE_API void xl6ExecutionFailuresRecordV1(XL6_ExecutionFailuresV1*,bool,struct xerror*);
XL6_CODE_LEASE_API bool xl6ExecutionFailuresSnapshotV1(XL6_ExecutionFailuresV1*,XL6_ExecutionFailureSnapshotV1*);
/* Checked BOOL carrier; failure may have no diagnostic. Caller owns the
 * ordinary error boundary. Unreported is for retirement, not ordinary wait. */
XL6_CODE_LEASE_API bool xl6ExecutionFailuresCheckV1(XL6_ExecutionFailuresV1*);
XL6_CODE_LEASE_API bool xl6ExecutionFailuresUnreportedV1(XL6_ExecutionFailuresV1*);
XL6_CODE_LEASE_API struct xrtownershipref xl6ExecutionFailuresOwnershipV1(const XL6_ExecutionFailuresV1*);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ExecutionFailuresAdapterV1(struct xrtownershipref);
/* Permanent exact native edge capability; initializer-safe typed identity. */
extern XL6_CODE_LEASE_API const struct xrtownershipops xl6ExecutionFailuresOpsV1;
#endif
