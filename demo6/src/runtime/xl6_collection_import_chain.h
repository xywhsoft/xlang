#ifndef XL6_COLLECTION_IMPORT_CHAIN_H
#define XL6_COLLECTION_IMPORT_CHAIN_H
#include "xl6_sequence.h"

/* Optional resident family adapter in the existing Array import transaction.
 * No source/generated callbacks or stored registry. Begin may publish an owned
 * context even on failure: End must release it. A successful context pins an
 * empty projected native template and owns its full immutable admission.
 * Predicates are allocation/error transparent; Append does checked admission.
 * All contexts and plan tables are synchronous, never stored in a result. */
typedef struct XL6_CollectionImportInputV1 {
    const xvalue* const* items;
    const int64_t* keys; /* NULL means contiguous keys, never a sparse-key cast. */
    size_t count;
    ptr context;
} XL6_CollectionImportInputV1;
typedef struct XL6_CollectionImportProviderV1 {
    bool (*Begin)(const xvalue*,ptr*);
    void (*End)(ptr);
    xvalue* (*Empty)(ptr);
    bool (*Admits)(ptr,const xvalue*);
    bool (*Append)(xvalue*,int64_t,const xvalue*);
    /* LANGUAGE_INPUT only: an exact family snapshot, not compact persistence
     * data. Begin may publish context on failure; End always retires it. Owned
     * item pins outlive callbacks; no mutable slot or template pin escapes. */
    bool (*InputBegin)(const xvalue*,XL6_CollectionImportInputV1*);
    void (*InputEnd)(XL6_CollectionImportInputV1*);
    /* Optional exact nested language-erasure probe, template context borrowed.
     * Publish required only on checked success. Matching native children keep
     * reference semantics (including BUSY); wrong typed/foreign owners do not
     * gain conversion authority by inspecting their contents. Resident only. */
    bool (*InputConversion)(ptr,const xvalue*,bool*);
} XL6_CollectionImportProviderV1;

enum { XL6_COLLECTION_IMPORT_RESTORE_V1=1u, XL6_COLLECTION_IMPORT_JSON_V1=2u,
    XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1=4u };
typedef struct XL6_CollectionImportPlanV1 {
    size_t size;
    /* A non-NULL table has count entries. NULL table/entries use the real resident Array family. Non-NULL entries
     * select only that provider; they cannot fall back to a different family. */
    const XL6_CollectionImportProviderV1* const* providers;
    unsigned int flags,leafRule;
    uint64_t leafIdentity;
    bool leafNullable;
} XL6_CollectionImportPlanV1;

/* Ordered homogeneous type chain, outermost first, 1..65 real empty seeds.
 * Each adjacent template must pass the parent's full resident admission.
 * Every seed is cleared/consumed on both paths when pointer/count is valid,
 * including a malformed plan or an upstream NULL factory failure. Source is
 * borrowed, never retagged or mutated; success returns an independent native
 * root (the original Array-only API retains its original root-seed contract).
 * Wire ARRAY input represents both Array and the approved compact List format.
 * LANGUAGE_INPUT instead selects a provider's exact sparse/native input
 * snapshot. It cannot be combined with RESTORE/JSON, never falls back to wire
 * ARRAY after family refusal, and preserves every int64 key. NULL providers
 * continue to use the resident Array input at descendant schema levels.
 * Same source Value at the same schema depth shares one native result. A native
 * Array with the authentic resident ANY policy is an erased language input:
 * a mismatched descendant Array schema explicitly imports it in that memo.
 * Already matching or statically typed native children keep reference semantics;
 * wrong typed/nominal/native families are not content-based coercions.
 * reused under another depth/domain is deliberately a distinct conversion.
 * RESTORE explicitly normalizes leaf wire scalars to the declared domain;
 * without it every input already needs exact native admission. Native nested
 * children keep their actual identities and full schema, never guessed casts.
 * Root/source aliases, duplicate seeds, BUSY/nonempty/unprojected templates,
 * cycles in traversed wire edges, and a mismatched family fail closed. */
XL6_TEXT_API xvalue* xl6CollectionImportChainTakeV1(xvalue**,size_t,const xvalue*,
    const XL6_CollectionImportPlanV1*);
XL6_TEXT_API xvalue* xl6CollectionImportChainSourceTakeV1(xvalue**,size_t,xvalue*,
    const XL6_CollectionImportPlanV1*);
#endif
