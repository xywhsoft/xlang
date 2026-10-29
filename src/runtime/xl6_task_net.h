#ifndef XL6_TASK_NET_H
#define XL6_TASK_NET_H
#include "xl6_task.h"
struct xnetengine;
struct xtasknetdataownershipv1;
/* Data is a shared task frame, not a generated callback or a pool job.
 * Failure leaves its single private owner for Discard. Submission never
 * redirects to a computation pool, spawns a helper thread or borrows a timer.
 * delayed uses the fixed native absolute microsecond clock; no ABI upgrade. */
XL6_CODE_LEASE_API struct xfuture* xl6TaskNetSubmitOwnedPolicyV1(struct xnetengine*,
    uint64_t,void*,struct xcancel*,const struct xfuturepayloadownershipv1*,bool,uint64_t);
XL6_CODE_LEASE_API const struct xtasknetdataownershipv1* xl6TaskNetDataPolicyV1Get(void);
#endif
