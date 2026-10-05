#ifndef XL6_SEQUENCE_H
#define XL6_SEQUENCE_H
#include "src/runtime/xl6_text_value.h"

/* Native-only shared owner for contiguous typed storage. The opaque token
 * pins one wrapper and closes its graph while an operation runs. No raw
 * element borrows or parameter write-back are exposed to xlang. */
typedef struct XL6_Sequence XL6_Sequence;
/* Private immutable resident policy. No function/context from generated code
 * may be stored here. Data owns its identity metadata independently of the
 * source module; it has no owning Value edges. Retain/Drop share that metadata
 * across clones and snapshots. Slot/Value predicates are allocation-free and
 * error-transparent. Empty policies preserve the low-level unprofiled ABI. */
typedef struct XL6_SequenceAdmissionOpsV1 {
    bool (*Retain)(const void*);
    void (*Drop)(const void*);
    bool (*Slot)(const void*, const void*);
    bool (*Value)(const void*, const xvalue*);
    bool (*Equivalent)(const void*, const void*);
    bool (*Defaultable)(const void*);
    /* Optional per-growth default. This is a resident callback, not source
     * code; it initializes a zeroed inactive slot or cleans up on failure. */
    bool (*InitDefault)(const void*, ptr);
} XL6_SequenceAdmissionOpsV1;
typedef struct XL6_SequenceAdmissionV2 {
    const XL6_SequenceAdmissionOpsV1* ops;
    const void* data;
    uint64_t elementIdentity;
    unsigned int transfer;
    size_t size;
    const xrttype* type;
    /* Required nonzero logical primitive identity on owned boxed projections.
     * Zero preserves a retained reference's actual identity (ANY/nominal).
     * This is copied with the policy, never recovered from a type spelling. */
    uint64_t boxedIdentity;
} XL6_SequenceAdmissionV2;
/* Copy returns an owned metadata reference; Take consumes valid policies on
 * both paths (a malformed foreign descriptor remains the caller's property).
 * Binding is only allowed on an Array/Cursor with no prior policy and
 * validates existing slots before publication.
 * These functions require an operation pin. They do not retain source code.
 * V2 includes projection identity in the copied owner metadata. The V1 record
 * entry points are retired: a stale smaller record must not reach this ABI. */
XL6_TEXT_API bool xl6SequenceAdmissionCopyV2(XL6_Sequence*, XL6_SequenceAdmissionV2*);
XL6_TEXT_API bool xl6SequenceAdmissionTakeV2(XL6_Sequence*, XL6_SequenceAdmissionV2*);
XL6_TEXT_API void xl6SequenceAdmissionDropV2(XL6_SequenceAdmissionV2*);
XL6_TEXT_API bool xl6SequenceAdmitSlotV1(XL6_Sequence*, const void*);
XL6_TEXT_API bool xl6SequenceAdmitValueV1(XL6_Sequence*, const xvalue*);
/* Valid only under an operation pin. Absent nullable projections keep the
 * canonical null identity; the returned ID is for present primitive values. */
XL6_TEXT_API uint64_t xl6SequenceBoxedIdentityV1(XL6_Sequence*);
XL6_TEXT_API bool xl6SequenceAdmissionEquivalentV1(XL6_Sequence*, XL6_Sequence*);
XL6_TEXT_API bool xl6SequenceAdmissionDefaultableV1(XL6_Sequence*);
XL6_TEXT_API bool xl6SequenceResizeStorageV1(XL6_Sequence*, size_t);
typedef enum XL6_SequenceRoleV1 {
    XL6_SEQUENCE_ARRAY_V1=1, XL6_SEQUENCE_STACK_V1, XL6_SEQUENCE_CURSOR_V1
} XL6_SequenceRoleV1;
typedef struct XL6_SequenceLayoutV1 {
    uint64_t identity, elementIdentity;
    unsigned int transfer;
    size_t size;
    const xrttype* type;
} XL6_SequenceLayoutV1;
/* Native-only cursor state belongs to the counted Sequence wrapper, not a
 * second owning node. Access is valid only inside a Begin/End operation pin.
 * No slot pointer is cached: snapshot storage is the sole element owner. */
typedef struct XL6_SequenceIteratorStateV1 {
    size_t next, current;
    bool prepared, ended, currentValid;
} XL6_SequenceIteratorStateV1;
XL6_TEXT_API XL6_SequenceIteratorStateV1* xl6SequenceIteratorStateV1(XL6_Sequence*);
/* A nullable managed carrier may use the C ABI's NULL slot or XRT's immortal
 * null value. They denote one absent value; never send a NULL C pointer to
 * xrtValueEqual, which requires two actual xvalue instances. */
static inline bool xl6SequenceReferenceEqualV1(const xvalue* left, const xvalue* right)
{
    if (left == right) return true;
    if (!left || !right) return xrtValueType(left ? left : right) == XVALUE_NULL;
    return xrtValueEqual(left,right);
}
XL6_TEXT_API bool xl6SequenceTupleV1(uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API const xrttype* xl6SequenceStorageTypeV1(unsigned int, const xrttype*);
/* Consumes valid owned storage on both success and failure. */
XL6_TEXT_API xvalue* xl6SequenceWrapV1(xtypedarray*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API xvalue* xl6SequenceWrapAsV1(xtypedarray*, XL6_SequenceRoleV1, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API XL6_Sequence* xl6SequenceBeginRoleV1(const xvalue*, XL6_SequenceRoleV1, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
/* Erasure is a reference alias, never a dynamic Value snapshot. The converter
 * is an immutable resident policy with module-independent lifetime and NULL
 * Context (like the stored resident xrttype), not a borrowed call converter.
 * Binding requires an ARRAY operation pin. A different second policy fails.
 * Probe returns 0 for another carrier/role without touching the error; 1 for
 * an admitted ARRAY; -1 for invalid/unbound/BUSY ARRAY state. */
XL6_TEXT_API bool xl6SequenceBindProjectionV1(XL6_Sequence*, const xvalueconverter*);
XL6_TEXT_API int xl6SequenceBeginErasedArrayV1(const xvalue*, XL6_Sequence**, XL6_SequenceLayoutV1*, const xvalueconverter**);
XL6_TEXT_API XL6_Sequence* xl6SequenceBeginV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API XL6_Sequence* xl6SequenceIteratorBeginV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API XL6_Sequence* xl6SequenceBeginOtherV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, XL6_Sequence*);
XL6_TEXT_API xtypedarray* xl6SequenceStorageV1(XL6_Sequence*);
XL6_TEXT_API void xl6SequenceEndV1(XL6_Sequence*);
XL6_TEXT_API bool xl6SequenceEqualsV1(const xvalue*, const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API bool xl6SequenceEqualsRoleV1(const xvalue*, const xvalue*, XL6_SequenceRoleV1, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API const xrtownershipadapterv1* xl6SequenceOwnershipAdapterV1(xrtownershipref);
#endif
