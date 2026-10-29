#ifndef XL6_THREAD_LOCAL_H
#define XL6_THREAD_LOCAL_H
#include "xl6_code_lease.h"
struct xrtownershipref;
struct xrtownershipadapterv1;
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ThreadLocalOwnershipAdapterV1(struct xrtownershipref);
#endif
