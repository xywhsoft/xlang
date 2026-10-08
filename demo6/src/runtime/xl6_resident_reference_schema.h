#ifndef XL6_RESIDENT_REFERENCE_SCHEMA_H
#define XL6_RESIDENT_REFERENCE_SCHEMA_H
#include "src/runtime/xl6_sequence.h"

/* Native provider-to-provider interface, never a generated descriptor. These
 * callbacks belong to resident components; Data owns only immutable metadata,
 * no Value/storage edges, source functions or borrowed module identity spans.
 * Depth counts recursive metadata records, including mixed container families.
 * The envelope has one resident admission identity, so its depth is recognized
 * without linking every optional child provider into scalar Array/Stack. */
typedef struct XL6_ResidentReferenceSchemaOpsV1 {
    void (*Drop)(const void*);
    bool (*Admits)(const void*, const xvalue*);
    bool (*Equivalent)(const void*, const void*);
} XL6_ResidentReferenceSchemaOpsV1;
typedef struct XL6_ResidentReferenceSchemaV1 {
    const XL6_ResidentReferenceSchemaOpsV1* ops;
    const void* data;
    uint64_t identity;
    unsigned int depth;
} XL6_ResidentReferenceSchemaV1;

/* Leaf resident policies have depth zero. Recursive policies use only this
 * envelope; no ABI extension or callback is inferred from a foreign ops table. */
XL6_TEXT_API unsigned int xl6ResidentAdmissionDepthV1(const XL6_SequenceAdmissionV3*);
/* Consumes a valid owned schema on both paths. A malformed descriptor remains
 * the caller's property. Only nullable elements permit absent default growth. */
XL6_TEXT_API xvalue* xl6SequenceResidentReferenceCreateTakeV1(unsigned int, uint64_t,
    uint64_t, XL6_ResidentReferenceSchemaV1*, bool);
#endif
