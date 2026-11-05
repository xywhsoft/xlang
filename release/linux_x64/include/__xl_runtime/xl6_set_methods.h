#ifndef XL6_SET_METHODS_H
#define XL6_SET_METHODS_H
#include "xl6_set.h"
/* Exact genuine native Set operations. No wire fallback or payload cast.
 * All operands borrowed except AddTake, which consumes its item on BOTH
 * outcomes. Add returns operation success (duplicates are normal success).
 * Missing membership/removal is false, not a failed checked operation.
 * Nullable counts publish only on success. Outputs are compiler-private. */
XL6_TEXT_API bool xl6SetValueAddTakeV1(const xvalue*,xvalue*);
XL6_TEXT_API bool xl6SetValueHasV1(const xvalue*,const xvalue*);
XL6_TEXT_API bool xl6SetValueRemoveV1(const xvalue*,const xvalue*);
XL6_TEXT_API bool xl6SetValueClearV1(const xvalue*);
XL6_TEXT_API bool xl6SetValueReserveV1(const xvalue*,int64_t);
XL6_TEXT_API bool xl6SetValueTrimV1(const xvalue*);
XL6_TEXT_API int64_t xl6SetValueCountV1(const xvalue*);
XL6_TEXT_API int64_t xl6SetValueCapacityV1(const xvalue*);
XL6_TEXT_API bool xl6SetValueCountIfPresentV1(const xvalue*,int64_t*);
XL6_TEXT_API xvalue* xl6SetValueCountOrNullV1(const xvalue*);
XL6_TEXT_API xvalue* xl6SetValueCloneV1(const xvalue*);
XL6_TEXT_API xvalue* xl6SetValueAlgebraV1(const xvalue*,const xvalue*,unsigned int);
XL6_TEXT_API bool xl6SetValueRelationV1(const xvalue*,const xvalue*,unsigned int,bool);
#endif
