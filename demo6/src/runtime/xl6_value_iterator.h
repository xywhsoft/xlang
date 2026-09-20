#ifndef XL6_VALUE_ITERATOR_H
#define XL6_VALUE_ITERATOR_H
#include <stdint.h>
#include "xl6_code_lease.h"

struct xvalue;
typedef struct xlir_xvalue_iterator XL6_ValueIterator;
/* Borrowed projection of the current snapshot item. The cursor owns the key
 * bytes and item; neither is another owning edge. Valid until advance/destroy.
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
typedef struct XL6_ValueIteratorRuntimeV1 {
    XL6_ValueIterator* (*Create)(const struct xvalue*);
    bool (*Advance)(XL6_ValueIterator*);
    const XL6_ValueIteratorView* (*View)(const XL6_ValueIterator*);
    void (*Destroy)(XL6_ValueIterator*);
    struct xrtownershipref (*Ownership)(const XL6_ValueIterator*);
} XL6_ValueIteratorRuntimeV1;
XL6_CODE_LEASE_API const XL6_ValueIteratorRuntimeV1* xl6ValueIteratorRuntimeV1Get(void);
/* Certifies only this resident carrier. Its actual managed XRT cursor has an
 * independent ValueCursor adapter; legacy stack/unique cursors do not. The
 * resolver must admit that cursor and all backing/finalizer children too. */
struct xrtownershipadapterv1;
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ValueIteratorOwnershipAdapterV1(struct xrtownershipref);
#endif
