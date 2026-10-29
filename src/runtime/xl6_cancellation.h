#ifndef XL6_CANCELLATION_H
#define XL6_CANCELLATION_H
#include "xl6_code_lease.h"
struct xcancelwatchownershipv1;
struct xrtownershipref;
struct xrtownershipadapterv1;
struct xrtownershipfinishv1;
struct xrtownershipquiescencev1;
XL6_CODE_LEASE_API const struct xcancelwatchownershipv1* xl6CancellationWatchPolicyV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6CancellationRegistrationAdapterV1(
    struct xrtownershipref, const struct xrtownershipfinishv1**);
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6CancellationRegistrationQuiescenceV1Get(void);
#endif
