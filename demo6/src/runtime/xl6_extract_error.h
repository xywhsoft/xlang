#ifndef XL6_EXTRACT_ERROR_H
#define XL6_EXTRACT_ERROR_H
#include "src/runtime/xl6_array_extract.h"
static inline void xl6ExtractKeepFirst(xerror** first)
{
    xerror* next=xrtTakeError();
    if (!*first) *first=next;
    else xrtErrorFree(next);
}
static inline void xl6ExtractRelease(xvalue* value)
{
    xerror* prior=xrtTakeError(); xrtValueRelease(value);
    if (prior) { xrtClearError(); xrtSetErrorTake(prior); }
}
#endif
