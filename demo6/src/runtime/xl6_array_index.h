#ifndef XL6_ARRAY_INDEX_H
#define XL6_ARRAY_INDEX_H
#include "src/runtime/xl6_sequence.h"
/* Resolve against a pinned count. Never negate INT64_MIN or add signed
 * indices to unsigned counts. Failure leaves the caller's output unchanged. */
static inline bool xl6ArrayIndexTryV1(int64_t index, size_t count, bool allowEnd, size_t* result)
{
    size_t resolved;
    if (!result) return false;
    if (index<0) {
        uint64_t distance=(uint64_t)(-(index+1))+UINT64_C(1);
        if (distance>(uint64_t)count) return false;
        resolved=count-(size_t)distance;
    } else {
        if ((uint64_t)index>(uint64_t)SIZE_MAX) return false;
        resolved=(size_t)index;
    }
    if (resolved>count || (!allowEnd && resolved==count)) {
        return false;
    }
    *result=resolved; return true;
}
/* Safe indexing uses the same resolver without manufacturing/clearing a
 * diagnostic on an ordinary miss. Both paths leave failed outputs untouched. */
static inline bool xl6ArrayIndexV1(int64_t index, size_t count, bool allowEnd, size_t* result)
{
    if (xl6ArrayIndexTryV1(index,count,allowEnd,result)) return true;
    xrtSetErrorKind(result ? XERR_RANGE : XERR_ARGUMENT); return false;
}
#endif
