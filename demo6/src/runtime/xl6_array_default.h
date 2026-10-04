#ifndef XL6_ARRAY_DEFAULT_H
#define XL6_ARRAY_DEFAULT_H
#include "src/runtime/xl6_value_default_abi.h"
#include "src/runtime/xl6_text_value.h"
/* Current boxed-array carrier, NOT the native Sequence ABI. Grow prepares a
 * delta and commits through XRT's failure-atomic Extend. The same shell/aliases
 * survive; validation/allocation failure leaves contents/count/capacity
 * unchanged. A destructor error after committed shrink does not roll back. */
XL6_TEXT_API bool xl6ArrayResizeDefaultV1(xvalue*,int64_t,unsigned int);
#endif
