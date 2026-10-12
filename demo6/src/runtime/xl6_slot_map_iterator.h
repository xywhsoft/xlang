#ifndef XL6_SLOT_MAP_ITERATOR_H
#define XL6_SLOT_MAP_ITERATOR_H
#include "xl6_slot_map.h"
#include "xl6_value_iterator.h"

/* Exact sealed factories. Paired capture is one source transaction; the other
 * forms select only their required snapshot lanes. Count-only captures the
 * initial count and performs no key/value copies. No source borrow escapes. */
XL6_CODE_LEASE_API XL6_ValueIterator* xl6SlotMapPairIteratorCreateV1(const xvalue*,
    const XL6_SlotMapLayoutV1*,uint64_t,const xvalueconverter*);
XL6_CODE_LEASE_API XL6_ValueIterator* xl6SlotMapKeyIteratorCreateV1(const xvalue*,
    const XL6_SlotMapLayoutV1*,uint64_t);
XL6_CODE_LEASE_API XL6_ValueIterator* xl6SlotMapItemIteratorCreateV1(const xvalue*,
    const XL6_SlotMapLayoutV1*,const xvalueconverter*);
XL6_CODE_LEASE_API XL6_ValueIterator* xl6SlotMapCountIteratorCreateV1(const xvalue*,
    const XL6_SlotMapLayoutV1*);
#endif
