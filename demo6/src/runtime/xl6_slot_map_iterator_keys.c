#include "xl6_slot_map_iterator_internal.h"
XL6_ValueIterator* xl6SlotMapKeyIteratorCreateV1(const xvalue* source,
    const XL6_SlotMapLayoutV1* layout,uint64_t keyIdentity)
{
    XL6_SlotMapSnapshotPairV1 pair={.keys=xl6SlotMapKeysV1(source,layout,keyIdentity)};
    if (!pair.keys) return NULL;
    XL6_SequenceLayoutV1 key=xl6SlotMapIteratorKeyLayoutV1(keyIdentity);
    return xl6SlotMapIteratorWrapTakeV1(&pair,&key,NULL,SIZE_MAX,xl6SlotMapKeyIteratorOpsV1Get());
}
