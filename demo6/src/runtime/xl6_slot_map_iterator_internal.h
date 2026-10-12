#ifndef XL6_SLOT_MAP_ITERATOR_INTERNAL_H
#define XL6_SLOT_MAP_ITERATOR_INTERNAL_H
#include "xl6_slot_map_iterator.h"
#include "xl6_value_iterator_internal.h"
/* Resident factories only. Consumes both valid owned lanes on every path.
 * SIZE_MAX derives count from the admitted arrays; a no-lane cursor supplies
 * the checked source count. Immutable layouts and resident ops are copied,
 * never caller-owned descriptors or generated callbacks. */
XL6_CODE_LEASE_API XL6_ValueIterator* xl6SlotMapIteratorWrapTakeV1(
    XL6_SlotMapSnapshotPairV1*,const XL6_SequenceLayoutV1*,const XL6_SequenceLayoutV1*,
    size_t,const XL6_ValueIteratorNativePairOpsV1*);
XL6_CODE_LEASE_API bool xl6SlotMapIteratorAdvanceSnapshotV1(
    const XL6_ValueIteratorNativePairSnapshotV1*,size_t,bool*,xvalue**);
XL6_CODE_LEASE_API const XL6_ValueIteratorNativePairOpsV1* xl6SlotMapItemIteratorOpsV1Get(void);
XL6_CODE_LEASE_API const XL6_ValueIteratorNativePairOpsV1* xl6SlotMapKeyIteratorOpsV1Get(void);
XL6_CODE_LEASE_API XL6_SequenceLayoutV1 xl6SlotMapIteratorKeyLayoutV1(uint64_t);
#endif
