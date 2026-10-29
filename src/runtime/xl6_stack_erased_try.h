#ifndef XL6_STACK_ERASED_TRY_H
#define XL6_STACK_ERASED_TRY_H
#include "xl6_sequence.h"

/* Compiler-private dynamic multi-result boundary. Borrows source; result must
 * be a valid initialized empty call vector. On success result owns exactly
 * (found: Bool, item: Value); absent and a present nullable-null stay distinct.
 * Failure leaves result empty and propagates the independent failure status.
 * Preparation precedes pop; a late destructor/publication failure may follow
 * committed removal, but cannot publish a partial result or leak its owners. */
XL6_TEXT_API bool xl6StackErasedTryCallV1(const xvalue* source,int64_t depth,bool pop,xrtcallresult* result);
#endif
