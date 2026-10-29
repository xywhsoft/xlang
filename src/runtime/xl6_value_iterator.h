#ifndef XL6_VALUE_ITERATOR_H
#define XL6_VALUE_ITERATOR_H
#include <stdint.h>
#include "xl6_code_lease.h"
#include "xl6_slot_map_key_abi.h"

struct xvalue;
typedef struct xlir_xvalue_iterator XL6_ValueIterator;
/* Borrowed projection of the current snapshot item. Wire cursors own their
 * key/item; native snapshots keep one owned current projection on this carrier.
 * Neither View field is another owning edge. Valid until advance/destroy.
 * Language aliases borrow the unique owning handle and serialize its use. */
typedef struct XL6_ValueIteratorView {
    struct xvalue* item;
    size_t ordinal;
    struct {
        int Type;
        union {
            size_t Index;
            int64_t Integer;
            struct { const char* Data; size_t Size; } String;
        };
    } key;
} XL6_ValueIteratorView;
typedef struct XL6_ValueIteratorRuntimeV2 {
    XL6_ValueIterator* (*Create)(const struct xvalue*);
    /* Status and end-of-stream are independent: false is failure, never EOF.
     * On success *found distinguishes an item from the terminal snapshot.
     * Failure remains observable if allocating its diagnostic also fails. */
    bool (*Advance)(XL6_ValueIterator*, bool* found);
    const XL6_ValueIteratorView* (*View)(const XL6_ValueIterator*);
    void (*Destroy)(XL6_ValueIterator*);
    struct xrtownershipref (*Ownership)(const XL6_ValueIterator*);
} XL6_ValueIteratorRuntimeV2;
XL6_CODE_LEASE_API const XL6_ValueIteratorRuntimeV2* xl6ValueIteratorRuntimeV2Get(void);
/* New sealed paired-snapshot subinterface; the V2 record and View layout are
 * unchanged. Copies the complete current key into an EMPTY output. A failed
 * projection invalidates the cursor; EOF/before-first/wrong-family calls are
 * refused without publishing a key. No box/unbox intermediate or key borrow. */
XL6_CODE_LEASE_API bool xl6ValueIteratorKeyCopyV3(XL6_ValueIterator*,XL6_SlotMapKeyV1*);
/* Logical Array with erased physical storage. Admit a sealed native ARRAY
 * snapshot or wire ARRAY only, never another native role or wire family.
 * Uses the same V2 carrier and introduces no optional List dependency. */
XL6_CODE_LEASE_API XL6_ValueIterator* xl6ArrayValueIteratorCreateCheckedV1(const struct xvalue*);
/* Set/Dict accept genuine native snapshots and their real wire families;
 * unrelated handles are refused. Each family is a separate component. */
XL6_CODE_LEASE_API XL6_ValueIterator* xl6SetValueIteratorCreateCheckedV1(const struct xvalue*);
XL6_CODE_LEASE_API XL6_ValueIterator* xl6DictValueIteratorCreateCheckedV1(const struct xvalue*);
/* Certifies only this resident carrier. The resolver must also admit the real
 * wire ValueCursor or native Sequence snapshot shell, its owned current value
 * projection, and all storage/finalizer children. No native handle is a leaf. */
struct xrtownershipadapterv1;
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ValueIteratorOwnershipAdapterV1(struct xrtownershipref);
#endif
