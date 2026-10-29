#ifndef XL6_SET_VALUE_ITERATOR_H
#define XL6_SET_VALUE_ITERATOR_H
#include "xl6_value_iterator.h"
/* Native Set and real wire Set, never an unrelated HANDLE payload. The
 * shared iterator owns a stable snapshot and its current owned projection. */
XL6_CODE_LEASE_API XL6_ValueIterator* xl6SetValueIteratorCreateCheckedV1(const struct xvalue*);
struct XL6_ValueIteratorNativeOpsV1;
XL6_CODE_LEASE_API const struct XL6_ValueIteratorNativeOpsV1* xl6SetValueIteratorNativeOpsV1Get(void);
#endif
