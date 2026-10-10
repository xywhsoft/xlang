#ifndef XL6_LIST_VALUE_ITERATOR_H
#define XL6_LIST_VALUE_ITERATOR_H
#include "xl6_value_iterator.h"
/* Compiler-private create entries; the common V2 carrier supplies View,
 * Advance, Destroy and ownership. Static List admits List/IntMap only; erased
 * foreach additionally admits real native Array and the four wire families.
 * Neither entry interprets a foreign HANDLE payload or treats failure as EOF. */
XL6_CODE_LEASE_API XL6_ValueIterator* xl6ListValueIteratorCreateV1(const struct xvalue*);
XL6_CODE_LEASE_API XL6_ValueIterator* xl6CollectionValueIteratorCreateV1(const struct xvalue*);
#endif
