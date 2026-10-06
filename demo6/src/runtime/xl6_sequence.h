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
typedef struct XL6_SequenceAdmissionOpsV2 {
    bool (*Retain)(const void*);
    void (*Drop)(const void*);
    bool (*Slot)(const void*, const void*);
    bool (*Value)(const void*, const xvalue*);
    bool (*Equivalent)(const void*, const void*);
    bool (*Defaultable)(const void*);
    /* Optional per-growth default. This is a resident callback, not source
     * code; it initializes a zeroed inactive slot or cleans up on failure. */
    bool (*InitDefault)(const void*, ptr);
    /* Optional native-reference edge, under the graph's frozen observation.
     * Required for FUTURE_REF; the resident policy visits the actual control
     * block, never an invented Value shell. Other transfers leave it NULL. */
    bool (*TraceSlot)(const void*, const void*, xrtownershipvisitor, void*);
} XL6_SequenceAdmissionOpsV2;
typedef struct XL6_SequenceAdmissionV3 {
    const XL6_SequenceAdmissionOpsV2* ops;
    const void* data;
    uint64_t elementIdentity;
    unsigned int transfer;
    size_t size;
    const xrttype* type;
    /* Required nonzero logical primitive identity on owned boxed projections.
     * Zero preserves a retained reference's actual identity (ANY/nominal).
     * This is copied with the policy, never recovered from a type spelling. */
    uint64_t boxedIdentity;
} XL6_SequenceAdmissionV3;
/* Copy returns an owned metadata reference; Take consumes valid policies on
 * both paths (a malformed foreign descriptor remains the caller's property).
 * Binding is only allowed on an Array/Stack/Cursor with no prior policy and
 * validates existing slots before publication.
 * These functions require an operation pin. They do not retain source code.
 * V3 requires the larger V2 ops record with native-reference tracing. V1/V2
 * entry points are retired; a stale smaller ops record cannot reach this ABI. */
XL6_TEXT_API bool xl6SequenceAdmissionCopyV3(XL6_Sequence*, XL6_SequenceAdmissionV3*);
XL6_TEXT_API bool xl6SequenceAdmissionTakeV3(XL6_Sequence*, XL6_SequenceAdmissionV3*);
XL6_TEXT_API void xl6SequenceAdmissionDropV3(XL6_SequenceAdmissionV3*);
XL6_TEXT_API bool xl6SequenceAdmitSlotV1(XL6_Sequence*, const void*);
XL6_TEXT_API bool xl6SequenceAdmitValueV1(XL6_Sequence*, const xvalue*);
/* Valid only under an operation pin. Absent nullable projections keep the
 * canonical null identity; the returned ID is for present primitive values. */
XL6_TEXT_API uint64_t xl6SequenceBoxedIdentityV1(XL6_Sequence*);
XL6_TEXT_API bool xl6SequenceAdmissionEquivalentV1(XL6_Sequence*, XL6_Sequence*);
XL6_TEXT_API bool xl6SequenceAdmissionDefaultableV1(XL6_Sequence*);
XL6_TEXT_API bool xl6SequenceResizeStorageV1(XL6_Sequence*, size_t);
/* Both owners are pinned. Target is an already-created empty ARRAY with its
 * final owned admission/projection metadata. Source is ARRAY or STACK with
 * the same element/physical tuple. All range/policy checks and allocations
 * precede ownership movement. A take only extracts the tail (depth zero).
 * Success publishes top-down order without allocating a post-move wrapper;
 * failure preserves both counts, capacity, storage and target metadata. */
XL6_TEXT_API bool xl6SequenceExtractTailV1(XL6_Sequence*, XL6_Sequence*, size_t, size_t, bool);
typedef enum XL6_SequenceRoleV1 {
    XL6_SEQUENCE_ARRAY_V1=1, XL6_SEQUENCE_STACK_V1, XL6_SEQUENCE_CURSOR_V1
} XL6_SequenceRoleV1;
enum { XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1 = 4 };
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
    bool prepared, ended, currentValid, failed;
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
/* Error-transparent resident domain probe, not an element borrow. Requires a
 * sealed ARRAY owner with a published immutable admission policy and exact
 * logical identity. BUSY is allowed: no storage or mutable policy is read.
 * Private ops are checked before dereferencing a foreign handle pointer. */
XL6_TEXT_API bool xl6SequenceAdmitsArrayV1(const xvalue*, uint64_t);
/* Exact immutable role/physical tuple probe. Like the erased Array probe it
 * is error-transparent and permits BUSY, without reading slots or callbacks. */
XL6_TEXT_API bool xl6SequenceAdmitsRoleV1(const xvalue*, XL6_SequenceRoleV1, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API const xrttype* xl6SequenceStorageTypeV1(unsigned int, const xrttype*);
/* Consumes valid owned storage on both success and failure. */
XL6_TEXT_API xvalue* xl6SequenceWrapV1(xtypedarray*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API xvalue* xl6SequenceWrapAsV1(xtypedarray*, XL6_SequenceRoleV1, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
/* One construction/publication transaction for ARRAY/STACK. A nonempty valid
 * resident policy supplies the exact element/physical tuple and is consumed
 * on both paths, including invalid role/owner identity. Malformed descriptors
 * remain the caller's property. No unprofiled owner escapes on failure.
 * STACK shares XRT's typed-array storage, not a second allocator/container. */
XL6_TEXT_API xvalue* xl6SequenceCreateAdmittedV1(XL6_SequenceRoleV1, uint64_t, XL6_SequenceAdmissionV3*);
/* Clone ARRAY/STACK storage and its immutable admission metadata together.
 * Existing low-level unprofiled owners remain unprofiled; a profiled owner
 * cannot lose its domain through clone. Source is unchanged on failure. */
XL6_TEXT_API xvalue* xl6SequenceCloneRoleV1(const xvalue*, XL6_SequenceRoleV1, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
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
/* Native-reference TraceSlot is authorized separately by exact resident ops
 * identity BEFORE Count/Trace. V1 refuses those wrappers; an absent or wrong
 * allowlist is not permission to ignore children or execute unknown callbacks. */
XL6_TEXT_API const xrtownershipadapterv1* xl6SequenceOwnershipAdapterV2(xrtownershipref,
    const XL6_SequenceAdmissionOpsV2* const*, size_t);
#endif
