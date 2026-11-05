#ifndef XL6_DICT_VALUE_ITERATOR_H
#define XL6_DICT_VALUE_ITERATOR_H
#include "xl6_value_iterator.h"
/* Static Dict uses only this family provider plus the shared carrier. */
struct XL6_ValueIteratorNativeOpsV1;
XL6_CODE_LEASE_API const struct XL6_ValueIteratorNativeOpsV1* xl6DictValueIteratorNativeOpsV1Get(void);
#endif
