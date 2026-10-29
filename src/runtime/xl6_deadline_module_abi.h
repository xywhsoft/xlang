#ifndef XL6_DEADLINE_MODULE_ABI_H
#define XL6_DEADLINE_MODULE_ABI_H
#include <stdbool.h>
#include <stddef.h>
#include "xl6_code_lease.h"
/* Generated modules own an opaque source. Native ticket/atomic layouts and
 * implementation include roots are not part of their physical ABI. */
typedef struct XL6_DeadlineServiceV1 XL6_DeadlineServiceV1;
struct xrtownershipref;
XL6_CODE_LEASE_API XL6_DeadlineServiceV1* xl6DeadlineServiceCreateV1(size_t);
XL6_CODE_LEASE_API void xl6DeadlineServiceCloseV1(XL6_DeadlineServiceV1*);
XL6_CODE_LEASE_API bool xl6DeadlineServiceDestroyV1(XL6_DeadlineServiceV1*);
XL6_CODE_LEASE_API bool xl6DeadlineServiceDestroyCompletedV1(XL6_DeadlineServiceV1*);
XL6_CODE_LEASE_API struct xrtownershipref xl6DeadlineServiceOwnershipV1(const XL6_DeadlineServiceV1*);
#endif
