#ifndef XL6_PERSISTENCE_RESTORE_PLAN_H
#define XL6_PERSISTENCE_RESTORE_PLAN_H
#include "xl6_collection_import_chain.h"
#include "xl6_value_admission_abi.h"
#include "xl6_ownership_read_abi.h"

enum { XL6_RESTORE_SCALAR_V1=1, XL6_RESTORE_SEQUENCE_V1, XL6_RESTORE_OBJECT_V1 };
enum { XL6_RESTORE_NULLABLE_V1=1u, XL6_RESTORE_FIELD_INSTANCE_V1=2u };
enum { XL6_RESTORE_REQUIRED_V1=1u, XL6_RESTORE_IN_PLACE_V1=2u };
/* Exact resident family predicate; never a generated/source callback. */
typedef struct XL6_RestoreObjectProviderV1 {
    bool (*IsNative)(const xvalue*);
    bool (*Admits)(const xvalue*,uint64_t,int64_t);
    /* Read-only resident policy admission BEFORE physical Count/Trace. Every
     * graph participant, including native children/code owners, must pass.
     * TypeId/field markers and ordinary generated callbacks are not proof. */
    bool (*OwnershipAdmits)(struct xrtownershipref);
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
    /* Compiler-selected declared clone/copy capability, not structural
     * inference or a second initializer. Borrows the committed receiver and
     * returns one owned result; runs outside all ownership fences. NULL means
     * checked rejection when the actual receiver is externally shared. */
    xvalue* (*clone)(const xvalue*);
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
    /* Optional separately bound RESIDENT host capability. Only surrounds
     * atomic field commit, never constructors, clone or cleanup callbacks.
     * NULL preserves closed resident-family admission; unknown owners refuse. */
    const XL6_OwnershipReadBoundaryV1* readBoundary;
} XL6_RestorePlanV1;

/* One transaction/memo for all object fields and collection elements. The
 * key is (source Value, explicit node, actual FIELD_INSTANCE child), not
 * recursion depth, nominal ID or a static field recipe alone. The actual child
 * has its own real memo pin, so pointer reuse cannot manufacture identity.
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
 * Fields are prepared without receiver writes and published as ONE checked
 * batch only after resident whole-graph admission and actual slot privacy.
 * A shared receiver uses only its declared clone capability, then the entire
 * prepared graph is rebuilt and reauthenticated. Initializers run once;
 * returned self or a still-shared cloned receiver is a checked failure.
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
/* Consumes the parser's owned source/error and the same construction inputs.
 * Only authenticated syntax/number or ordinary file-access rejection becomes
 * nullable wire rejection. A failed parser without a diagnostic stays failed.
 * Source retirement participates in the checked first-error transaction. */
XL6_TEXT_API bool xl6PersistenceRestoreParsedTakeV1(xvalue**,size_t,xvalue*,
    xvalue*,bool,xerror*,bool,xerror*,bool,const XL6_RestorePlanV1*,xvalue**);
#endif
