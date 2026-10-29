#ifndef XL6_PROMISE_H
#define XL6_PROMISE_H
#include "xl6_code_lease.h"
struct xrtownershipref;
struct xrtownershipadapterv1;
struct xrtownershipfinishv1;
struct xrtownershipquiescencev1;
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6PromiseOwnershipAdapterV1(
    struct xrtownershipref,const struct xrtownershipfinishv1**);
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6PromiseQuiescenceV1Get(void);
#endif
