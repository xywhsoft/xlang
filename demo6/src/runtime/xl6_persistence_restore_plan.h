#ifndef XL6_PERSISTENCE_RESTORE_PLAN_H
#define XL6_PERSISTENCE_RESTORE_PLAN_H
#include "xl6_collection_import_chain.h"
#include "xl6_value_admission_abi.h"

enum { XL6_RESTORE_SCALAR_V1=1, XL6_RESTORE_SEQUENCE_V1, XL6_RESTORE_OBJECT_V1 };
enum { XL6_RESTORE_NULLABLE_V1=1u, XL6_RESTORE_FIELD_INSTANCE_V1=2u };
enum { XL6_RESTORE_REQUIRED_V1=1u, XL6_RESTORE_IN_PLACE_V1=2u };
/* Exact resident family predicate; never a generated/source callback. */
typedef struct XL6_RestoreObjectProviderV1 {
    bool (*IsNative)(const xvalue*);
    bool (*Admits)(const xvalue*,uint64_t,int64_t);
} XL6_RestoreObjectProviderV1;

/* Closed compiler-private construction domains, not a type-name registry.
 * Reusing a declaration node selects the same restoration/default domain.
 * FIELD_INSTANCE nodes require an actual initialized private child and cannot
 * have a factory or be nullable. Each distinct actual child is a distinct
 * instance domain, even for the same static field recipe. Distinct field
 * recipes still require distinct nodes. Tables/factories are borrowed synchronously.
 * A factory is the actual lowered declaration initializer, not a recursive
 * generated restore function. It returns an independent owned, committed
 * language object; the plan is never retained by the result. */
typedef struct XL6_RestoreNodeV1 {
    unsigned int kind,flags;
    uint64_t identity;
    union {
        unsigned int scalar;
        struct { size_t seed,element; const XL6_CollectionImportProviderV1* provider; } sequence;
        struct { xvalue* (*factory)(void); int64_t runtimeTag; size_t first,count;
            const XL6_RestoreObjectProviderV1* provider; } object;
    } as;
} XL6_RestoreNodeV1;
typedef struct XL6_RestoreFieldV1 {
    xstrview source,target;
    size_t node;
    unsigned int flags;
} XL6_RestoreFieldV1;
typedef struct XL6_RestorePlanV1 {
    size_t size,nodeCount,fieldCount,root;
    const XL6_RestoreNodeV1* nodes;
    const XL6_RestoreFieldV1* fields;
    bool json;
} XL6_RestorePlanV1;

/* One transaction/memo for all object fields and collection elements. The
 * key is (source Value, explicit node, actual FIELD_INSTANCE child), not
 * recursion depth, nominal ID or a static field recipe alone. The actual child
 * is pinned by its memo result, so pointer reuse cannot manufacture identity.
 * Collection templates are genuine empty projected resident Array/List
 * objects. They are consumed on both paths for a valid pointer/count pair.
 * target is an optional owned, PRIVATE declaration-default root and is
 * consumed on both paths; only a declaration OBJECT root permits it. IN_PLACE
 * edges must select FIELD_INSTANCE nodes; replacement/sequence edges cannot.
 * The supplied capability is validated before wire rejection or memo reuse.
 * IN_PLACE selects
 * the actual initialized private child, without running its initializer a
 * second time. These are construction capabilities, not mutation of an
 * arbitrary externally shared object. All object outputs must match their
 * exact nominal identity and language runtime tag before any field write.
 * Missing optional fields preserve defaults; explicit null is distinct.
 * Source is borrowed wire ARRAY/OBJECT/scalar, never retagged or mutated.
 * Traversed cycles and more than 65 object/collection levels fail closed.
 * Failure withdraws the whole unpublished result, but cannot undo ordinary
 * language initializer/destructor side effects. No public language ABI yet. */
XL6_TEXT_API xvalue* xl6PersistenceRestorePlanTakeV1(xvalue**,size_t,xvalue*,
    const xvalue*,const XL6_RestorePlanV1*);
/* Nullable read shares the SAME transaction and construction-instance memo. A proven
 * wire shape/range/missing-required-field/cycle rejection returns true with
 * an owned canonical null, after checked retirement of every partial result.
 * Constructors, providers, codecs, OOM, recursion budget and cleanup failures
 * return false even without an allocated diagnostic; error KIND never selects
 * benign rejection. Invalid plans/JSON capabilities are checked failures too.
 * output must be empty and must not alias the consumed seed table. A nonalias
 * occupied output is preserved on refusal; valid owned inputs are consumed.
 * NULL source is an upstream failure, not a benign parser rejection. */
XL6_TEXT_API bool xl6PersistenceRestorePlanReadTakeV1(xvalue**,size_t,xvalue*,
    const xvalue*,const XL6_RestorePlanV1*,xvalue**);
#endif
