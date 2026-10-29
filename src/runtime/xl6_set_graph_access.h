#ifndef XL6_SET_GRAPH_ACCESS_H
#define XL6_SET_GRAPH_ACCESS_H
#include "xl6_set.h"
/* Resident provider access while XRT guards the source Value shell. Only
 * Begin constructs this pinned view; do not retain it, copy it, or reacquire
 * the guarded shell. An ops match precedes every payload dereference. */
typedef struct XL6_SetGraphViewV1 {
    void* owner;
    XL6_SetLayoutV1 layout;
    xtypedset* storage;
} XL6_SetGraphViewV1;
XL6_TEXT_API int xl6SetGraphBeginV1(const xvaluehandleops*,ptr,ptr,uint64_t,XL6_SetGraphViewV1*);
XL6_TEXT_API void xl6SetGraphEndV1(XL6_SetGraphViewV1*);
XL6_TEXT_API xvalue* xl6SetGraphEmptyV1(const XL6_SetGraphViewV1*);
/* Non-reference slots only. Reference edges are borrowed from pinned slots
 * and passed directly to Next in the one shared identity memo. */
XL6_TEXT_API xvalue* xl6SetGraphProjectSlotV1(const XL6_SetGraphViewV1*,const void*);
#endif
