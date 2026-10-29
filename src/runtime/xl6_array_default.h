#ifndef XL6_ARRAY_DEFAULT_H
#define XL6_ARRAY_DEFAULT_H
#include "xl6_value_default_abi.h"
#include "xl6_text_value.h"
/* Borrowed receiver. An admitted/bound native ARRAY uses its sealed resident
 * default policy and XRT's typed resize transaction; abi is only the wire
 * carrier's verified language-default hint, never a native slot constructor.
 * Recognized unbound/BUSY native arrays fail without wire fallback. Wire grow
 * prepares a delta and commits through failure-atomic Extend. Both preserve
 * aliases and contents/count/capacity on validation/allocation failure.
 * A destructor error after committed shrink does not roll back. */
XL6_TEXT_API bool xl6ArrayResizeDefaultV1(xvalue*,int64_t,unsigned int);
#endif
