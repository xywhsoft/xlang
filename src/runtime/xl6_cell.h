#ifndef XL6_CELL_H
#define XL6_CELL_H
#include <stddef.h>
#include "xl6_code_lease.h"

/* Opaque, resident cell storage. Generated modules never own its descriptor
 * code or inspect its layout. A cell does not itself admit generated execution;
 * captured closures/frames and any owned payload supply their real code edges. */
typedef struct xlir_cell XL6_Cell;
typedef enum XL6_CellKind {
    XL6_CELL_BOOL, XL6_CELL_I64, XL6_CELL_F64, XL6_CELL_STRING,
    XL6_CELL_XVALUE, XL6_CELL_U64, XL6_CELL_TRIVIAL_BYTES, XL6_CELL_TYPE_ID,
    XL6_CELL_CHAR, XL6_CELL_NATIVE_LEAF_V2
} XL6_CellKind;
typedef struct XL6_CellRuntimeV1 {
    /* Create is fallible and owns no input. Trivial bytes are a closed,
     * pointer-free language carrier, not arbitrary native/managed storage. */
    XL6_Cell* (*Create)(XL6_CellKind, size_t);
    /* Initialize once, consuming the supplied owner for STRING/XVALUE/native.
     * No retain, allocation or destruction occurs at this commit. */
    void (*Initialize)(XL6_Cell*, const void*, size_t);
    /* Borrowed observation. The caller owns a cell ref and serializes ordinary
     * accesses. Freeze participation is not a concurrent-variable lock. */
    void (*Load)(const XL6_Cell*, void*, size_t);
    /* Store accepts only initialized trivial payloads. Exchange is the single
     * owning replacement operation: take the new owner, return the old owner
     * without destroying it. Input/output byte ranges must be disjoint. */
    void (*Store)(XL6_Cell*, const void*, size_t);
    void (*Exchange)(XL6_Cell*, const void*, void*, size_t);
    XL6_Cell* (*Retain)(XL6_Cell*);
    void (*Release)(XL6_Cell*);
    struct xrtownershipref (*Ownership)(const XL6_Cell*);
} XL6_CellRuntimeV1;
/* Every mutation/last-release tail participates before touching cell state.
 * View is read-only and still needs whole-graph quiescence, including children.
 * Mis-sized/mis-typed operations are contract violations, not fallible writes. */
XL6_CODE_LEASE_API const XL6_CellRuntimeV1* xl6CellRuntimeV1Get(void);
struct xrtownershipadapterv1;
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6CellOwnershipAdapterV1(struct xrtownershipref);
/* Private permanent native leaf strategy. No generated callback, language
 * edge or code lease; metadata is immutable and resident. Validate is pure
 * and error-transparent; Destroy empties and drops outside our mutation. */
typedef struct XL6_CellLeafOpsV1 {
    bool (*Validate)(const void*,size_t);
    void (*Destroy)(const void*,void*);
} XL6_CellLeafOpsV1;
XL6_CODE_LEASE_API XL6_Cell* xl6CellCreateLeafV1(size_t,const XL6_CellLeafOpsV1*,const void*);
#endif
