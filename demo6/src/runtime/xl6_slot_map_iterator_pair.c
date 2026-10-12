#include "xl6_slot_map_iterator_internal.h"
XL6_ValueIterator* xl6SlotMapPairIteratorCreateV1(const xvalue* source,
    const XL6_SlotMapLayoutV1* layout,uint64_t keyIdentity,const xvalueconverter* converter)
{
    XL6_SlotMapSnapshotPairV1 pair={0};
    if (!xl6SlotMapPairsV1(source,layout,keyIdentity,converter,&pair)) return NULL;
    XL6_SequenceLayoutV1 key=xl6SlotMapIteratorKeyLayoutV1(keyIdentity);
    return xl6SlotMapIteratorWrapTakeV1(&pair,&key,&layout->value,SIZE_MAX,xl6SlotMapKeyIteratorOpsV1Get());
}
