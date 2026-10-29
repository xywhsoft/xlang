#include "xl6_slot_map_iterator_internal.h"
XL6_ValueIterator* xl6SlotMapItemIteratorCreateV1(const xvalue* source,
    const XL6_SlotMapLayoutV1* layout,const xvalueconverter* converter)
{
    XL6_SlotMapSnapshotPairV1 pair={.values=xl6SlotMapValuesV1(source,layout,converter)};
    if (!pair.values) return NULL;
    return xl6SlotMapIteratorWrapTakeV1(&pair,NULL,&layout->value,SIZE_MAX,xl6SlotMapItemIteratorOpsV1Get());
}
