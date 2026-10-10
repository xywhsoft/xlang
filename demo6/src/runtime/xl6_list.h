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
/* Capture only owned immutable metadata from a genuine live List, including
 * an operation-pinned owner. Does not traverse/retain storage or require an
 * empty source. Cursor/foreign/cleared owners fail closed. The admission
 * output must be empty; both outputs remain unchanged on failure. */
XL6_TEXT_API bool xl6ListSchemaCopyV1(const xvalue*,XL6_ListLayoutV1*,XL6_SequenceAdmissionV3*);
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

/* Explicit immutable resident projection; NULL Context and complete codec.
 * Transfer 3 uses the canonical retained-reference codec, not a foreign one.
 * The codec's code/record must be resident, never generated module callbacks.
 * Erase retains the SAME shell and storage without copying/retagging. Take
 * consumes the factory result on both paths. A bound codec cannot be changed.
 * Clone preserves the binding; Cursor cannot serve as an erased List. */
XL6_TEXT_API xvalue* xl6ListEraseV1(const xvalue*,const XL6_ListLayoutV1*,const xvalueconverter*);
XL6_TEXT_API xvalue* xl6ListProjectionTakeV1(xvalue*,const XL6_ListLayoutV1*,const xvalueconverter*);
/* Private owned read outputs must initially be NULL; found is distinct from
 * checked success. Missing is normal, failure leaves both outputs unchanged.
 * Take removes only after successful encoding, identity binding and admission.
 * Dynamic writes validate the full resident Value domain before decoding. */
XL6_TEXT_API bool xl6ListErasedReadV1(const xvalue*,int64_t,bool,xvalue**,bool*);
XL6_TEXT_API bool xl6ListErasedSetV1(const xvalue*,int64_t,const xvalue*);
XL6_TEXT_API bool xl6ListErasedAppendV1(const xvalue*,const xvalue*,int64_t*);
/* Checked erased methods authenticate the private owner and its bound resident
 * projection, never just HANDLE/type ID. Missing keys and zero count are normal;
 * private outputs are published only on success. No schema/layout guessing. */
XL6_TEXT_API bool xl6ListErasedCountV1(const xvalue*,int64_t*);
XL6_TEXT_API bool xl6ListErasedKeyV1(const xvalue*,int64_t,bool,bool*);
XL6_TEXT_API bool xl6ListErasedClearV1(const xvalue*);
/* Resident family dispatch only: 0 unrelated, -1 getter failure, 1 exact
 * private handle ops (including invalid/retired/cursor owners). Never reads a
 * foreign payload or admits a collection; the selected operation must still
 * authenticate layout, projection and the actual owner pin. */
XL6_TEXT_API int xl6ListErasedProbeV1(const xvalue*);
/* Value equality matches erased Array search: unlike writes, a query need not
 * belong to the element domain. Every stored slot is admitted/projected while
 * pinned, then compared as a Value. No whole-list boxing or wire conversion.
 * Checked failure (also without a diagnostic) leaves the output unchanged. */
XL6_TEXT_API bool xl6ListErasedContainsV1(const xvalue*,const xvalue*,bool*);
XL6_TEXT_API bool xl6ListErasedTrimV1(const xvalue*,int64_t,int64_t*);
/* 0 unrelated (no output writes), -1 checked failure, 1 publishes a full typed
 * snapshot plus its immutable layout. The explicit private cursor identity must
 * differ from both List and witness identities. The owned target starts NULL.
 * No wire conversion or source callback is retained. */
XL6_TEXT_API int xl6ListIteratorCreateErasedV1(const xvalue*,uint64_t,xvalue**,XL6_ListLayoutV1*);
/* Each successful step publishes one owned projection and the complete int64
 * key; EOF publishes found=false, item=NULL, key=0. Private item starts NULL.
 * Failure leaves outputs unchanged and invalidates the snapshot when advancement
 * or projection has begun. Repeated EOF is normal; failure is never EOF. */
XL6_TEXT_API bool xl6ListIteratorAdvanceValueV1(const xvalue*,const XL6_ListLayoutV1*,uint64_t,
    bool*,xvalue**,int64_t*);
/* Explicit one-level sparse wire snapshot, not var erasure or a recursive
 * persistence implementation. Complete int64 keys and owned projected values
 * are retained. Nested native children require the compiler's closed graph
 * plan; this layer does not guess child schemas or flatten them into ANY. */
XL6_TEXT_API xvalue* xl6ListToWireV1(const xvalue*);
/* Consumes a private empty projected seed on BOTH paths. Same-domain native
 * source + identical resident codec copies typed storage without boxing.
 * Otherwise explicit conversion observes a stable IntMap snapshot. Native
 * mismatches use an independent one-level wire projection before conversion.
 * Every Value is admitted by the final destination policy; source identity,
 * slots and keys are never rebound or mutated. Same-owner source aliases and
 * nonempty/unprojected seeds fail closed. No published replacement semantics. */
XL6_TEXT_API xvalue* xl6ListImportTakeV1(xvalue*,const xvalue*);

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
 * Nested metadata and graph projection use optional resident components;
 * language cutover remains pending: this is not yet the source list carrier. */
XL6_TEXT_API const xrtownershipadapterv1* xl6ListOwnershipAdapterV1(xrtownershipref);
XL6_TEXT_API const xrtownershipadapterv1* xl6ListOwnershipAdapterV2(xrtownershipref,
    const XL6_SequenceAdmissionOpsV2* const*,size_t);
#endif
