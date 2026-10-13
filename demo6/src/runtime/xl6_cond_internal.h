#ifndef XL6_COND_INTERNAL_H
#define XL6_COND_INTERNAL_H
#include "xl6_sync_internal.h"
/* Exact-status absolute wait for resident composition. Timeout is normal;
 * failures preserve their checked boundary and both native activity pins.
 * Included only by providers already selecting XRT's condition/wait types. */
XL6_SYNC_INTERNAL xwaitresult xl6CondWaitUntilNative(const struct xvalue*,uint64_t,
    const struct xvalue*,uint64_t,xdeadline);
#endif
