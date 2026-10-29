#include "xl6_slot_map_iterator_internal.h"
XL6_ValueIterator* xl6SlotMapCountIteratorCreateV1(const xvalue* source,const XL6_SlotMapLayoutV1* layout)
{
    int64_t count=0;
    if (!xl6SlotMapSizeV1(source,layout,false,&count)) return NULL;
    if (count<0) { xrtSetErrorKind(XERR_STATE); return NULL; }
    XL6_SlotMapSnapshotPairV1 pair={0};
    return xl6SlotMapIteratorWrapTakeV1(&pair,NULL,NULL,(size_t)count,xl6SlotMapItemIteratorOpsV1Get());
}
