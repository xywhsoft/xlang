#ifndef XL6_ARRAY_INDEX_H
#define XL6_ARRAY_INDEX_H
#include "src/runtime/xl6_sequence.h"
/* Resolve against a pinned count. Never negate INT64_MIN or add signed
 * indices to unsigned counts. Failure leaves the caller's output unchanged. */
static inline bool xl6ArrayIndexV1(int64_t index, size_t count, bool allowEnd, size_t* result)
{
    size_t resolved;
    if (!result) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (index<0) {
        uint64_t distance=(uint64_t)(-(index+1))+UINT64_C(1);
        if (distance>(uint64_t)count) { xrtSetErrorKind(XERR_RANGE); return false; }
        resolved=count-(size_t)distance;
    } else {
        if ((uint64_t)index>(uint64_t)SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }
        resolved=(size_t)index;
    }
    if (resolved>count || (!allowEnd && resolved==count)) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    *result=resolved; return true;
}
#endif
