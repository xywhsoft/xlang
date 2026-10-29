#ifndef XL6_DYNAMIC_COLLECTION_VECTOR_H
#define XL6_DYNAMIC_COLLECTION_VECTOR_H
#include "xl6_dynamic_collection.h"
#include "xl6_stack_erased_try.h"
/* Borrows receiver, consumes arguments on BOTH outcomes, owns complete vector.
 * Native multi-result operation identity is explicit, not a member name guess. */
XL6_TEXT_API bool xl6DynamicCollectionVectorTakeV1(xvalue*,unsigned,xvalue*,xvalue*,xrtcallresult*);
/* 1 ready Stack, 0 different family, -1 unavailable/error. No foreign HANDLE
 * payload cast; successful probe pins and unpins the authenticated owner. */
XL6_TEXT_API int xl6DynamicStackMethodProbeV1(const xvalue*);
XL6_TEXT_API bool xl6DynamicCollectionVectorArityV1(unsigned,size_t);
#endif
