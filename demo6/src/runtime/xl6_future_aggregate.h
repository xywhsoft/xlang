#ifndef XL6_FUTURE_AGGREGATE_H
#define XL6_FUTURE_AGGREGATE_H
#include <stddef.h>
#include "xl6_code_lease.h"
struct xfuture;
struct xvalue;
struct xrtownershipref;
struct xrtownershipadapterv1;
struct xfuturecombineownershipv1;
typedef struct xvalue* (*XL6_FutureAggregateBoxV1)(const void*);

/* One resident bridge to the native ALL/ANY producer. All borrows ordered
 * input slots (duplicates included); array forms unpack the verified xvalue
 * Future carrier in the same resident implementation. NULL Box explicitly
 * selects void All; otherwise it boxes one borrowed successful payload.
 * A non-NULL Box must be protected by the supplied module owner, or an
 * explicitly externally-resident NULL owner. Acquisition inherits only this
 * thread's accepted same-owner CodeScope, otherwise it needs fresh admission.
 * Caller pins its entry code through preparation and failure rollback.
 * NULL return consumes no input and invokes no boxer; every accepted callback,
 * result, cancellation and source cleanup follows the native producer path.
 * This table does not prove multi-input deadlock or grant cancellation rights. */
typedef struct XL6_FutureAggregateRuntimeV1 {
    struct xfuture* (*All)(struct xfuture* const*, size_t, XL6_FutureAggregateBoxV1, const XL6_CodeOwner*);
    struct xfuture* (*Race)(struct xfuture* const*, size_t);
    struct xfuture* (*WhenAnyIndex)(struct xfuture* const*, size_t);
    struct xfuture* (*AllArray)(const struct xvalue*, XL6_FutureAggregateBoxV1, const XL6_CodeOwner*);
    struct xfuture* (*RaceArray)(const struct xvalue*);
    struct xfuture* (*WhenAnyIndexArray)(const struct xvalue*);
} XL6_FutureAggregateRuntimeV1;
XL6_CODE_LEASE_API const XL6_FutureAggregateRuntimeV1* xl6FutureAggregateRuntimeV1Get(void);
/* Exact resident identity. Recognizing the native group does not admit this
 * mapper or its code owner; the host must independently resolve every node. */
XL6_CODE_LEASE_API const struct xfuturecombineownershipv1* xl6FutureAggregatePolicyV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6FutureAggregateOwnershipAdapterV1(struct xrtownershipref);
#endif
