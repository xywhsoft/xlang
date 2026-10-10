#ifndef XL6_VALUE_ITERATOR_INTERNAL_H
#define XL6_VALUE_ITERATOR_INTERNAL_H
#include "xl6_value_iterator.h"
#include "xl6_sequence.h"

/* Resident components only. Never publish a generated/module-owned callback
 * record here. The carrier owns Cursor and the current projected item; layouts
 * are immutable metadata and contain no owning storage or callback context. */
typedef struct XL6_ValueIteratorNativeSnapshotV1 {
    xvalue* cursor;
    XL6_SequenceLayoutV1 layout;
    const xvalueconverter* projection;
    uint64_t ownerIdentity;
    uint64_t cursorIdentity;
} XL6_ValueIteratorNativeSnapshotV1;
typedef struct XL6_ValueIteratorNativeOpsV1 {
    /* 0 unrelated, -1 checked failure, 1 owned snapshot. No partial output. */
    int (*Create)(const xvalue*, XL6_ValueIteratorNativeSnapshotV1*);
    bool (*Advance)(const XL6_ValueIteratorNativeSnapshotV1*, bool*, xvalue**, xvaluekey*);
} XL6_ValueIteratorNativeOpsV1;
/* Private cross-component entry. Only immutable resident records may be
 * supplied. No language/Cgen ABI accepts a user callback record. An unrelated
 * extension falls through to native Array and then the real wire cursor. */
XL6_CODE_LEASE_API XL6_ValueIterator* xl6ValueIteratorCreateResidentV1(
    const xvalue*, const XL6_ValueIteratorNativeOpsV1*);
#endif
