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
    /* Resident extension dispatch only; not a Value tag or admission token. */
    unsigned family;
} XL6_ValueIteratorNativeSnapshotV1;

enum { XL6_VALUE_ITERATOR_LIST_V1=1, XL6_VALUE_ITERATOR_SET_V1=2, XL6_VALUE_ITERATOR_DICT_V1=3 };
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
/* Separate versioned resident record, never an extension of NativeOpsV1.
 * Arrays are already independent native snapshots. One shared ordinal keeps
 * their rows aligned; no secondary storage clone or wire-container traversal.
 * Key-only/value-only/count-only variants own only the required lanes. */
enum { XL6_VALUE_ITERATOR_PAIR_KEY_V1=1, XL6_VALUE_ITERATOR_PAIR_ITEM_V1=2 };
typedef struct XL6_ValueIteratorNativePairSnapshotV1 {
    xvalue* keys;
    xvalue* values;
    XL6_SequenceLayoutV1 keyLayout,valueLayout;
    const xvalueconverter* valueProjection;
    size_t count;
    unsigned mode;
} XL6_ValueIteratorNativePairSnapshotV1;
typedef struct XL6_ValueIteratorNativePairOpsV1 {
    bool (*Advance)(const XL6_ValueIteratorNativePairSnapshotV1*,size_t,bool*,xvalue**);
    /* Owns no current key: copy directly from the pinned native key row.
     * On false the EMPTY output remains unchanged, including no-diagnostic
     * failures. Only immutable resident callbacks are permitted. */
    bool (*CopyKey)(const XL6_ValueIteratorNativePairSnapshotV1*,size_t,XL6_SlotMapKeyV1*);
} XL6_ValueIteratorNativePairOpsV1;
/* Consumes both valid owned lanes and clears the input on ALL paths. The
 * producer must finish sealed-array/layout/count admission before this entry. */
XL6_CODE_LEASE_API XL6_ValueIterator* xl6ValueIteratorCreatePairTakeV1(
    XL6_ValueIteratorNativePairSnapshotV1*,const XL6_ValueIteratorNativePairOpsV1*);
#endif
