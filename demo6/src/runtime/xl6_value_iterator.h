#ifndef XL6_VALUE_ITERATOR_H
#define XL6_VALUE_ITERATOR_H
#include <stdint.h>
#include "xl6_code_lease.h"

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
/* Certifies only this resident carrier. The resolver must also admit the real
 * wire ValueCursor or native Sequence snapshot shell, its owned current value
 * projection, and all storage/finalizer children. No native handle is a leaf. */
struct xrtownershipadapterv1;
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ValueIteratorOwnershipAdapterV1(struct xrtownershipref);
#endif
