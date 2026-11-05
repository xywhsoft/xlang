#ifndef XL6_LIST_GRAPH_ACCESS_H
#define XL6_LIST_GRAPH_ACCESS_H
#include "xl6_list.h"

/* Resident provider-to-owner access. Only Begin constructs a live view; do
 * not fabricate/copy it, retain it after End, or cross execution contexts.
 * The pin keeps immutable metadata and physical slots alive while XRT guards
 * the source Value shell. No source GetHandle/Retain is permitted there. */
typedef struct XL6_ListGraphViewV1 {
    void* owner;
    XL6_ListLayoutV1 layout;
    xtypedlist* storage;
} XL6_ListGraphViewV1;
XL6_TEXT_API int xl6ListGraphBeginV1(const xvaluehandleops*,ptr,ptr,uint64_t,XL6_ListGraphViewV1*);
XL6_TEXT_API void xl6ListGraphEndV1(XL6_ListGraphViewV1*);
/* Independent empty native target with the identical owned policy/codec;
 * no temporary Array/wire witness or container clone is constructed. */
XL6_TEXT_API xvalue* xl6ListGraphEmptyV1(const XL6_ListGraphViewV1*);
/* Non-reference slots only. Reference edges must be borrowed directly from
 * the pinned physical slot and passed to Next without retaining ancestors. */
XL6_TEXT_API xvalue* xl6ListGraphProjectSlotV1(const XL6_ListGraphViewV1*,const void*);
#endif
