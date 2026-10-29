#ifndef XL6_VALUE_OBJECT_FIELDS_H
#define XL6_VALUE_OBJECT_FIELDS_H
#include "xl6_code_lease.h"
typedef struct xvalue xvalue;
/* Internal borrowed lookup. Own the receiver and synchronize mutations while
 * using the result. NULL with no error is absence; stored canonical null is a
 * present value. Public erased indexing retains the result before returning.
 * Exact bytes, including NUL, route declared slots before authenticated open
 * storage. Unknown/corrupt native storage is a checked failure, not a miss. */
XL6_CODE_LEASE_API xvalue* xl6ValueObjectFieldFindV1(const xvalue*,const char*,size_t);
/* Borrow receiver; consume one independent item owner on BOTH outcomes.
 * Retire the replaced child outside XRT's mutation fence, with its receiver
 * pinned. Preserve the first operation/cleanup failure, including silent one. */
XL6_CODE_LEASE_API bool xl6ValueObjectFieldSetTakeV1(xvalue*,const char*,size_t,xvalue*);
#endif
