#ifndef XL6_ARRAY_ALGEBRA_H
#define XL6_ARRAY_ALGEBRA_H
#include "src/runtime/xl6_sequence.h"

/* Borrow both arrays and return a new owning array. Wire/wire keeps the Value
 * data ABI; native/native requires identical complete schemas and keeps the
 * Sequence ABI, owned admission and resident projection. Mixed carriers,
 * foreign handles, Stack, unprofiled/unbound and BUSY owners are rejected,
 * never implicitly converted or retagged. Both sources remain unchanged on
 * success and failure. The first native/callback error survives rollback. */
XL6_TEXT_API xvalue* xl6ArrayConcatV1(const xvalue*, const xvalue*);
#endif
