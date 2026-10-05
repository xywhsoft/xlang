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

/* Direct owned-mapper ABI. Native lists select the shared kernel; Array forms
 * also select the input adapter. NULL Box selects void All; otherwise the
 * supplied module owner (or an explicitly resident NULL owner) protects Box.
 * Scoped admission, producer cancellation and ownership graphs stay unchanged.
 * Input order and duplicate owning edges are preserved. NULL return consumes
 * no input and invokes no boxer; caller pins entry code through rollback.
 * No function table or borrowed Array/slot escapes the synchronous adapter.
 * These entries do not grant cancellation rights or certify deadlock. */
XL6_CODE_LEASE_API struct xfuture* xl6FutureAggregateAllV2(struct xfuture* const*, size_t,
    XL6_FutureAggregateBoxV1, const XL6_CodeOwner*);
XL6_CODE_LEASE_API struct xfuture* xl6FutureAggregateRaceV2(struct xfuture* const*, size_t);
XL6_CODE_LEASE_API struct xfuture* xl6FutureAggregateWhenAnyIndexV2(struct xfuture* const*, size_t);
XL6_CODE_LEASE_API struct xfuture* xl6FutureAggregateAllArrayV2(const struct xvalue*,
    XL6_FutureAggregateBoxV1, const XL6_CodeOwner*);
XL6_CODE_LEASE_API struct xfuture* xl6FutureAggregateRaceArrayV2(const struct xvalue*);
XL6_CODE_LEASE_API struct xfuture* xl6FutureAggregateWhenAnyIndexArrayV2(const struct xvalue*);
/* Exact resident identity. Recognizing the native group does not admit this
 * mapper or its code owner; the host must independently resolve every node. */
XL6_CODE_LEASE_API const struct xfuturecombineownershipv1* xl6FutureAggregatePolicyV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6FutureAggregateOwnershipAdapterV1(struct xrtownershipref);
#endif
