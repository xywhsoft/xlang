#ifndef XL6_TREE_H
#define XL6_TREE_H
#include "xl6_sequence.h"
#include "xl6_tree_key_abi.h"

/* Closed XLIR tuple. Array identities describe the two independently owned
 * admission domains, not Array storage kept inside the tree. The key order
 * uses XL6_TreeKeyOrderV1; values do not acquire a fake order. */
typedef struct XL6_TreeLayoutV1 {
    uint64_t identity;
    unsigned int keyOrder;
    XL6_SequenceLayoutV1 key, value;
} XL6_TreeLayoutV1;

/* Both fresh, admitted ARRAY witnesses are consumed on both paths. Only
 * immutable resident metadata is retained; no witness, generated descriptor,
 * source callback or source module is retained by the schema. */
XL6_TEXT_API xvalue* xl6TreeCreateTakeV1(const XL6_TreeLayoutV1*, xvalue*, xvalue*);
/* Capture two fresh empty ARRAY domains, not their storage. Outputs must be
 * empty and distinct; all outputs stay unchanged on failure. The borrowed
 * witnesses are not consumed. Admits is an error-transparent immutable probe:
 * actual private Tree ops precede pointer access, BUSY is legal, and complete
 * key/value policies as well as the physical/logical layout must match. */
XL6_TEXT_API bool xl6TreeDomainsCopyV1(const XL6_TreeLayoutV1*, const xvalue*, const xvalue*,
    XL6_SequenceAdmissionV3*, XL6_SequenceAdmissionV3*);
XL6_TEXT_API bool xl6TreeAdmitsSchemaV1(const xvalue*, const XL6_TreeLayoutV1*,
    const XL6_SequenceAdmissionV3*, const XL6_SequenceAdmissionV3*);
/* Captures both immutable policies from a genuine Tree, including a BUSY
 * owner, without reading slots or retaining storage. Independent empty
 * outputs are unchanged on failure; both policies own their metadata. */
XL6_TEXT_API bool xl6TreeSchemaCopyV1(const xvalue*,XL6_TreeLayoutV1*,
    XL6_SequenceAdmissionV3*,XL6_SequenceAdmissionV3*);
/* Consumes both ARRAY schema witnesses on every path. Only independently
 * owned metadata enters the parent; no temporary Tree storage is allocated. */
XL6_TEXT_API xvalue* xl6SequenceTreeCreateTakeV1(unsigned int, uint64_t, uint64_t,
    const XL6_TreeLayoutV1*, xvalue*, xvalue*, bool);
XL6_TEXT_API bool xl6TreeSetV1(const xvalue*, const XL6_TreeLayoutV1*, const void*, const void*);
/* The return is operation success; found/equal are data results. A missing
 * key, empty endpoint or unequal value is not an error. Initialized owning
 * output slots are unchanged on a missing result or a pre-publication failure. */
XL6_TEXT_API bool xl6TreeGetV1(const xvalue*, const XL6_TreeLayoutV1*, const void*, void*, bool, bool*);
XL6_TEXT_API bool xl6TreeKeyV1(const xvalue*, const XL6_TreeLayoutV1*, const void*, bool, bool*);
/* endpoint: 0=first, 1=last, 2=lower bound, 3=upper bound. Bounds require a
 * search key; first/last require NULL. Overlapping outputs yield found=false
 * without changing either output, preserving the existing language contract. */
XL6_TEXT_API bool xl6TreeEndpointV1(const xvalue*, const XL6_TreeLayoutV1*,
    unsigned int, const void*, void*, void*, bool*);
/* Fresh result slots, not caller-variable writeback. Their exact resident
 * storage type supplies alignment/copy/drop. Managed slots must be empty;
 * trivial slots are scratch. Pairs are distinct, disjoint from search/layout/
 * found, and have exact sizes. All initialized outputs are empty after a failed
 * operation or a missing optional result. Empty is the resident codec's zero
 * result state, not a default T (in particular no empty-string allocation).
 * found is published only on success;
 * invalid slot shapes leave every supplied byte untouched. The result slots
 * stay private until the return status has been checked. */
typedef enum XL6_TreeQueryV2 {
    XL6_TREE_QUERY_GET_V2, XL6_TREE_QUERY_TAKE_V2,
    XL6_TREE_QUERY_FIRST_V2, XL6_TREE_QUERY_LAST_V2,
    XL6_TREE_QUERY_LOWER_V2, XL6_TREE_QUERY_UPPER_V2
} XL6_TreeQueryV2;
typedef struct XL6_TreeQuerySlotV2 { void* data; size_t size; } XL6_TreeQuerySlotV2;
/* GET/TAKE have no key output; FIRST/LAST have no search key. Bounds have both.
 * required converts an ordinary missing result to XERR_RANGE, never a storage,
 * copy, state or allocation error to missing. TAKE moves the value, other
 * queries copy independently owned results. No temporary result allocation. */
XL6_TEXT_API bool xl6TreeQueryV2(const xvalue*, const XL6_TreeLayoutV1*,
    XL6_TreeQueryV2, const void*, bool, XL6_TreeQuerySlotV2,
    XL6_TreeQuerySlotV2, bool*);
XL6_TEXT_API bool xl6TreeEqualsV1(const xvalue*, const xvalue*, const XL6_TreeLayoutV1*, bool*);
XL6_TEXT_API bool xl6TreeMergeV1(const xvalue*, const xvalue*, const XL6_TreeLayoutV1*, bool);
XL6_TEXT_API bool xl6TreeClearV1(const xvalue*, const XL6_TreeLayoutV1*);
XL6_TEXT_API int64_t xl6TreeCountV1(const xvalue*, const XL6_TreeLayoutV1*);
XL6_TEXT_API int64_t xl6TreeTrimV1(const xvalue*, const XL6_TreeLayoutV1*, int64_t);
XL6_TEXT_API xvalue* xl6TreeCloneV1(const xvalue*, const XL6_TreeLayoutV1*);
/* Consumes the exact fresh projection ARRAY on both paths. The tree owner
 * remains operation-pinned through iteration, copying and target cleanup. */
XL6_TEXT_API xvalue* xl6TreeProjectTakeV1(const xvalue*, const XL6_TreeLayoutV1*, bool,
    xvalue*, const xvalueconverter*);
XL6_TEXT_API const xrtownershipadapterv1* xl6TreeOwnershipAdapterV1(xrtownershipref);
#endif
