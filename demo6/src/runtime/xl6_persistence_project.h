#ifndef XL6_PERSISTENCE_PROJECT_H
#define XL6_PERSISTENCE_PROJECT_H
#include "src/runtime/xl6_array_graph.h"

/* Verified XLIR emits immutable data, not a recursive C implementation.
 * Keys are byte views: renamed keys may contain NUL. Field order is the
 * resolved base-to-derived order; later target keys replace earlier keys.
 * An omitted field is never read and never entered into the graph memo. */
#define XL6_PERSIST_OMIT_NULL_V1  UINT32_C(1)
#define XL6_PERSIST_OMIT_EMPTY_V1 UINT32_C(2)
#define XL6_PERSIST_SCHEMA_DISABLED_V1 UINT32_C(1)
typedef struct XL6_PersistenceFieldV1 {
    xstrview source;
    xstrview target;
    uint32_t flags;
} XL6_PersistenceFieldV1;

/* Open-object storage is admitted by an exact native ops identity before
 * dereferencing its payload. This does not certify arbitrary opaque handles.
 * ProjectExtras is a resident provider; closed plans do not link its module. */
typedef struct XL6_PersistenceOpenV1 {
    xstrview storage;
    xstrview handle;
    const xvaluehandleops* ops;
} XL6_PersistenceOpenV1;
typedef struct XL6_PersistenceSchemaV1 {
    uint64_t identity;
    uint32_t flags;
    size_t fieldCount;
    const XL6_PersistenceFieldV1* fields;
    const XL6_PersistenceOpenV1* open;
} XL6_PersistenceSchemaV1;
typedef bool (*XL6_PersistenceExtrasV1)(const XL6_PersistenceOpenV1*,
    xvaluegraphnextv1*, xvalue*);
typedef struct XL6_PersistencePlanV1 {
    size_t size;
    size_t schemaCount;
    const XL6_PersistenceSchemaV1* schemas;
    XL6_PersistenceExtrasV1 projectExtras;
    const xvaluegraphsequencev1* sequences;
    size_t sequenceCount;
} XL6_PersistencePlanV1;

/* Borrowed source/plan; owned wire result or checked NULL. No global schema
 * registry, source mutation, retained generated callback, or lifetime hooks
 * in the result. Unknown objects bearing a language class marker fail closed
 * instead of silently serializing private/internal storage. */
XL6_TEXT_API xvalue* xl6PersistenceProjectV1(const xvalue*, const XL6_PersistencePlanV1*);
XL6_TEXT_API bool xl6PersistenceProjectOpenV1(const XL6_PersistenceOpenV1*,
    xvaluegraphnextv1*, xvalue*);
#endif
