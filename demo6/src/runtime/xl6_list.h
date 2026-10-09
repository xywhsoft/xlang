#ifndef XL6_LIST_H
#define XL6_LIST_H
#include "xl6_sequence.h"

/* Private native sparse-list storage boundary, not yet the source list<T>
 * carrier. The value ARRAY identity names an independently owned admission
 * domain, NOT an Array stored in the list. Keys are complete int64 values. */
typedef struct XL6_ListLayoutV1 {
    uint64_t identity;
    XL6_SequenceLayoutV1 value;
} XL6_ListLayoutV1;

/* Only a fresh empty admitted ARRAY supplies metadata. Take consumes that
 * witness on both paths; a List retains no witness/storage/source callback.
 * Copy requires an empty output and leaves it unchanged on failure. Probe
 * is error-transparent, verifies private ops before pointer access, permits
 * an operation-BUSY owner and compares the full immutable logical policy. */
XL6_TEXT_API bool xl6ListDomainCopyV1(const XL6_ListLayoutV1*,const xvalue*,XL6_SequenceAdmissionV3*);
XL6_TEXT_API bool xl6ListAdmitsSchemaV1(const xvalue*,const XL6_ListLayoutV1*,const XL6_SequenceAdmissionV3*);
XL6_TEXT_API xvalue* xl6ListCreateTakeV1(const XL6_ListLayoutV1*,xvalue*);
XL6_TEXT_API bool xl6ListSetV1(const xvalue*,const XL6_ListLayoutV1*,int64_t,const void*);
/* Append uses maximum key + 1 (empty -> 0), not element count. Overflow is
 * checked and does not insert. The actual key is published only on success. */
XL6_TEXT_API bool xl6ListAppendV1(const xvalue*,const XL6_ListLayoutV1*,const void*,int64_t*);
/* Checked operation success is separate from found. Missing keys are normal
 * and do not alter the initialized owned output. Copy failure before
 * publication preserves it; take moves the slot then removes exactly its key.
 * Outputs are compiler-private result slots, never source byref arguments. */
XL6_TEXT_API bool xl6ListGetV1(const xvalue*,const XL6_ListLayoutV1*,int64_t,void*,bool,bool*);
XL6_TEXT_API bool xl6ListKeyV1(const xvalue*,const XL6_ListLayoutV1*,int64_t,bool,bool*);
XL6_TEXT_API bool xl6ListClearV1(const xvalue*,const XL6_ListLayoutV1*);
XL6_TEXT_API int64_t xl6ListCountV1(const xvalue*,const XL6_ListLayoutV1*);
XL6_TEXT_API int64_t xl6ListTrimV1(const xvalue*,const XL6_ListLayoutV1*,int64_t);
XL6_TEXT_API xvalue* xl6ListCloneV1(const xvalue*,const XL6_ListLayoutV1*);
/* Checked success is distinct from equality/presence. Logical policy, full
 * tuple, count, complete sparse keys and value semantics define identity;
 * pool pages, insertion order and wrapper/storage addresses do not. Scalars
 * use their typed descriptor, references use Value equality/hash (no invented
 * total-order comparator), Futures use control-block reference identity.
 * All outputs are unchanged on failure. A Cursor is not a collection key.
 * Hash is the native payload domain; XRT ValueHash additionally mixes its
 * HANDLE tag. Do not compare the numeric hashes of different boundaries or
 * treat either as a stable wire format. Hashing is not a freeze API: callers
 * must keep keys stable while stored. */
XL6_TEXT_API bool xl6ListEqualsV1(const xvalue*,const xvalue*,const XL6_ListLayoutV1*,bool*);
XL6_TEXT_API bool xl6ListContainsV1(const xvalue*,const XL6_ListLayoutV1*,const void*,bool*);
XL6_TEXT_API bool xl6ListHashV1(const xvalue*,const XL6_ListLayoutV1*,uint64_t*);
/* XRT performs failure-atomic same-domain merge. false preserves target values
 * at duplicate keys; true replaces them. Self merge is a normal no-op. */
XL6_TEXT_API bool xl6ListMergeV1(const xvalue*,const xvalue*,const XL6_ListLayoutV1*,bool);

/* Cursor owns a full typed-list snapshot and immutable admission metadata,
 * independently of the source. It iterates stable private slots in O(n),
 * never by repeated ordinal lookup. Full/bounded forward/reverse traversal
 * share the XRT iterator. Cursor identity must differ from list identity.
 * A source List operation cannot accept a Cursor sharing its value tuple. */
XL6_TEXT_API xvalue* xl6ListCursorCreateV1(const xvalue*,const XL6_ListLayoutV1*,uint64_t,bool,bool,int64_t);
XL6_TEXT_API bool xl6ListCursorAdvanceV1(const xvalue*,const XL6_ListLayoutV1*,uint64_t,bool*);
XL6_TEXT_API bool xl6ListCursorReadV1(const xvalue*,const XL6_ListLayoutV1*,uint64_t,int64_t*,void*);
XL6_TEXT_API bool xl6ListCursorEndV1(const xvalue*,const XL6_ListLayoutV1*,uint64_t);

/* Phased graph retirement traces physical slots once (duplicates stay
 * duplicates). Future control-block tracing additionally requires an exact
 * resident admission-ops allowlist; V1 does not silently ignore those edges.
 * Erased wire bridges and language cutover remain pending: this private handle
 * is not yet the source list carrier, despite its completed identity hooks. */
XL6_TEXT_API const xrtownershipadapterv1* xl6ListOwnershipAdapterV1(xrtownershipref);
XL6_TEXT_API const xrtownershipadapterv1* xl6ListOwnershipAdapterV2(xrtownershipref,
    const XL6_SequenceAdmissionOpsV2* const*,size_t);
#endif
