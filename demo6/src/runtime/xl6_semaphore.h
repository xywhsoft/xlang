#ifndef XL6_SEMAPHORE_H
#define XL6_SEMAPHORE_H
#include "xl6_code_lease.h"
struct xvalue;
struct xrtownershipadapterv1;
struct xrtownershipref;

/* Resident, reference-valued semaphore. Clone aliases the same native owner;
 * no generated callback, module pointer or borrowed language edge is retained.
 * Blocking waits hold a real native activity reference, not a world mutation.
 * Constructors are checked; bool methods are no-raise status operations and
 * restore the caller's diagnostic, including ordinary timeout/full results. */
XL6_CODE_LEASE_API struct xvalue* xl6SemCreateV1(uint64_t, int64_t, int64_t);
XL6_CODE_LEASE_API struct xvalue* xl6SemCreateDefaultV1(uint64_t);
XL6_CODE_LEASE_API bool xl6SemWaitV1(const struct xvalue*, uint64_t);
XL6_CODE_LEASE_API bool xl6SemTryWaitV1(const struct xvalue*, uint64_t);
XL6_CODE_LEASE_API bool xl6SemWaitTimeoutV1(const struct xvalue*, uint64_t, int64_t);
XL6_CODE_LEASE_API bool xl6SemPostV1(const struct xvalue*, uint64_t);
XL6_CODE_LEASE_API bool xl6SemPostMultipleV1(const struct xvalue*, uint64_t, int64_t);
/* Read-only admission checks exact private handle provenance and immutable
 * type identity before dereferencing opaque data. No allocation/diagnostics.
 * A busy operation does not change the owner's type or invalidate admission. */
XL6_CODE_LEASE_API bool xl6SemAdmitsV1(const struct xvalue*, uint64_t);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6SemOwnershipAdapterV1(struct xrtownershipref);
#endif
