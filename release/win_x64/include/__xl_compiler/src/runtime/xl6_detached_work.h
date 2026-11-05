#ifndef XL6_DETACHED_WORK_H
#define XL6_DETACHED_WORK_H
#include "xl6_closure.h"
#include "xl6_execution_failures.h"
struct xrtownershipops;
struct xrtownershipquiescencev1;
struct xrtownershippreparationv1;
typedef struct XL6_DetachedWorkV1 XL6_DetachedWorkV1;
/* Exact nullary checked void closure. Preparation retains captures/code leases;
 * acceptance transfers this one work owner to its native producer. Rejection
 * Abort consumes the preparation without counting a failed/cancelled job.
 * Invoke and Drop are separate: the record remains BUSY through capture cleanup
 * and records one combined body/cleanup outcome. Drop(NULL context) is also the
 * normal cancellation path before Invoke. No producer algorithm is linked here. */
XL6_CODE_LEASE_API XL6_DetachedWorkV1* xl6DetachedWorkCreateV1(XL6_ExecutionFailuresV1*,XL6_Closure*);
XL6_CODE_LEASE_API XL6_DetachedWorkV1* xl6DetachedWorkCreateBoxedV1(XL6_ExecutionFailuresV1*,const struct xvalue*,uint64_t);
XL6_CODE_LEASE_API void xl6DetachedWorkInvokeV1(void*);
/* Accepted producer failed before invoking the body. Consumes the diagnostic
 * (NULL is still failure), remains BUSY through Drop, counts one combined
 * producer/capture-cleanup outcome. Not the ordinary cancellation path. */
XL6_CODE_LEASE_API void xl6DetachedWorkFailV1(void*,struct xerror*);
XL6_CODE_LEASE_API void xl6DetachedWorkDropV1(void*,void*);
XL6_CODE_LEASE_API void xl6DetachedWorkAbortV1(XL6_DetachedWorkV1*);
XL6_CODE_LEASE_API extern const struct xrtownershipops xl6DetachedWorkOpsV1;
XL6_CODE_LEASE_API const struct xrtownershipops* xl6DetachedWorkOpsV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6DetachedWorkQuiescenceV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6DetachedWorkOwnershipAdapterV1(
    struct xrtownershipref,const struct xrtownershippreparationv1**);
#endif
