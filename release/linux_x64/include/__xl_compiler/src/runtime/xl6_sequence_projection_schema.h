#ifndef XL6_SEQUENCE_PROJECTION_SCHEMA_H
#define XL6_SEQUENCE_PROJECTION_SCHEMA_H
#include "xl6_sequence.h"

/* Compiler-private owning metadata, not a container/reference language type.
 * Prepare requires an empty output. It captures the full immutable policy
 * without keeping the witness or its storage/code alive. Independent equivalent
 * policies compare by their declared contents, not pointer or TypeId alone.
 * A prepared record is immutable until Dispose; do not byte-copy its ownership. */
typedef struct XL6_SequenceProjectionSchemaV1 {
    XL6_SequenceRoleV1 role;
    XL6_SequenceLayoutV1 layout;
    XL6_SequenceAdmissionV3 admission;
} XL6_SequenceProjectionSchemaV1;

XL6_TEXT_API bool xl6SequenceProjectionPrepareV1(const xvalue*, XL6_SequenceRoleV1,
    XL6_SequenceProjectionSchemaV1*);
/* Borrowed source/schema; empty output owns the SAME source on success. No
 * retagging, policy rebinding, slot access, cloning or per-match allocation.
 * Operation-BUSY genuine owners remain legal. Nullable mismatch owns the
 * canonical Value null, not a failed native NULL. A real failure still returns
 * false even without a diagnostic, and never changes the output slot. */
XL6_TEXT_API bool xl6SequenceProjectionApplyV1(const XL6_SequenceProjectionSchemaV1*,
    const xvalue*, bool, xvalue**);
/* Consumes metadata and clears the record on both outcomes. Error status is
 * checked; the first failure survives cleanup, including an absent diagnostic. */
XL6_TEXT_API bool xl6SequenceProjectionDisposeV1(XL6_SequenceProjectionSchemaV1*);
/* Consumes a private factory witness on both outcomes; source is borrowed and
 * must be a distinct owner. The same checked projection pipeline is used by
 * generated code; successful output is not published until metadata cleanup. */
XL6_TEXT_API bool xl6SequenceProjectionWitnessTakeV1(xvalue*, XL6_SequenceRoleV1,
    const xvalue*, bool, xvalue**);
#endif
