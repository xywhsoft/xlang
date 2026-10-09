#ifndef XL6_ARRAY_SHUFFLE_H
#define XL6_ARRAY_SHUFFLE_H
#include "xl6_text_value.h"

/* Synchronous mutable Array consumer, not an Array snapshot/conversion.
 * Native storage is pinned and shuffled directly by XRT, without boxing,
 * allocation, descriptor callbacks or a retained RNG/module address. A native
 * Array must have its resident projection bound; foreign handles, other roles,
 * unbound and BUSY owners fail before consuming random words or changing slots.
 * Wire/value Arrays remain an explicit host/data boundary. Their COW detach
 * can fail; neither failure nor permutation promises rollback of RNG state.
 * The explicit RNG belongs to the caller; its language thread-affinity check
 * remains in the existing Random receiver adapter. Default RNG is thread-local.
 */
XL6_TEXT_API bool xl6ArrayRngShuffleV1(xrng*, const xvalue*);
XL6_TEXT_API bool xl6ArrayRandShuffleV1(const xvalue*);
#endif
