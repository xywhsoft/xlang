#ifndef XL6_DICT_H
#define XL6_DICT_H
#include "xl6_sequence.h"

/* Private native Dict foundation, not yet the source dict<T> carrier. Text
 * keys are copied exact byte spans by XRT typed_dict (including empty/NUL).
 * The ARRAY tuple names an owned immutable value domain, not stored wire data.
 * No generated descriptor, codec or callback is retained by this boundary. */
typedef struct XL6_DictLayoutV1 {
    uint64_t identity;
    XL6_SequenceLayoutV1 value;
} XL6_DictLayoutV1;

/* Fresh empty admitted ARRAY witness; Take consumes it on BOTH paths. Copy
 * outputs start empty and remain unchanged on failure. Schema capture does
 * not inspect slots and permits BUSY genuine owners, never a cursor/foreign
 * payload. Admission is error-transparent and checks the full logical policy. */
XL6_TEXT_API bool xl6DictDomainCopyV1(const XL6_DictLayoutV1*,const xvalue*,XL6_SequenceAdmissionV3*);
XL6_TEXT_API bool xl6DictSchemaCopyV1(const xvalue*,XL6_DictLayoutV1*,XL6_SequenceAdmissionV3*);
XL6_TEXT_API bool xl6DictAdmitsSchemaV1(const xvalue*,const XL6_DictLayoutV1*,const XL6_SequenceAdmissionV3*);
XL6_TEXT_API xvalue* xl6DictCreateTakeV1(const XL6_DictLayoutV1*,xvalue*);
XL6_TEXT_API bool xl6DictSetV1(const xvalue*,const XL6_DictLayoutV1*,xstrview,const void*);
/* Checked success is distinct from found; missing is normal. Output is a
 * compiler-private initialized EMPTY owned slot (POD payload may be nonzero).
 * An already-owned string/reference/Future output is rejected before lookup,
 * so replacing an old finalizer cannot fail after publication. Copy failure
 * preserves it; Take
 * moves only a found value and removes exactly its copied text key. */
XL6_TEXT_API bool xl6DictGetV1(const xvalue*,const XL6_DictLayoutV1*,xstrview,void*,bool,bool*);
XL6_TEXT_API bool xl6DictKeyV1(const xvalue*,const XL6_DictLayoutV1*,xstrview,bool,bool*);
XL6_TEXT_API bool xl6DictClearV1(const xvalue*,const XL6_DictLayoutV1*);
XL6_TEXT_API int64_t xl6DictCountV1(const xvalue*,const XL6_DictLayoutV1*);
XL6_TEXT_API int64_t xl6DictCapacityV1(const xvalue*,const XL6_DictLayoutV1*);
XL6_TEXT_API bool xl6DictReserveV1(const xvalue*,const XL6_DictLayoutV1*,int64_t);
XL6_TEXT_API bool xl6DictTrimV1(const xvalue*,const XL6_DictLayoutV1*);
XL6_TEXT_API xvalue* xl6DictCloneV1(const xvalue*,const XL6_DictLayoutV1*);
/* Exact layout AND complete policy are required before XRT's atomic merge.
 * Self merge is a no-op. No Value projection or temporary wire map is used. */
XL6_TEXT_API bool xl6DictMergeV1(const xvalue*,const xvalue*,const XL6_DictLayoutV1*,bool);

/* Independent typed snapshot, stable insertion order, forward/reverse O(n)
 * traversal. Cursor identity differs from owner/witness; owner methods reject
 * cursors. End is idempotent; EOF and sticky failure are distinct. Read's key
 * output starts {0}; value output is a distinct private initialized empty slot.
 * Overlapping key/value outputs are refused before either copy. Key and
 * value are owned copies, never borrowed source addresses or source byref.
 * A failed copy does not publish a key or invalidate the current item. */
XL6_TEXT_API xvalue* xl6DictCursorCreateV1(const xvalue*,const XL6_DictLayoutV1*,uint64_t,bool);
XL6_TEXT_API bool xl6DictCursorAdvanceV1(const xvalue*,const XL6_DictLayoutV1*,uint64_t,bool*);
XL6_TEXT_API bool xl6DictCursorReadV1(const xvalue*,const XL6_DictLayoutV1*,uint64_t,xstrview*,void*);
XL6_TEXT_API bool xl6DictCursorEndV1(const xvalue*,const XL6_DictLayoutV1*,uint64_t);

/* Bind one immutable resident context-free complete converter pair. NULL
 * selects XRT's ordinary builtin codec; language-owned string/nullable/nominal
 * carriers need their declared resident codec. It executes even when XRT's
 * physical builtin conversion would otherwise bypass a fallback converter.
 * Reference slots REQUIRE NULL
 * and use retained-reference projection instead of XRT graph cloning. Take
 * consumes on BOTH paths. Erasure retains the genuine shell/storage; it is
 * not wire conversion. A binding cannot subsequently be replaced. */
XL6_TEXT_API xvalue* xl6DictProjectionTakeV1(xvalue*,const XL6_DictLayoutV1*,const xvalueconverter*);
/* Probe: 0 unrelated, 1 private family (even invalid/unbound/busy/cursor),
 * -1 Value inspection failed. Never authorizes dereferencing a raw payload. */
XL6_TEXT_API int xl6DictErasedProbeV1(const xvalue*);
XL6_TEXT_API bool xl6DictErasedSetV1(const xvalue*,xstrview,const xvalue*);
/* Private result starts NULL. Found is distinct from checked success. A take
 * admits the final projection before removing its key; failure leaves both
 * outputs unchanged. Dynamic writes validate logical AND physical domains. */
XL6_TEXT_API bool xl6DictErasedReadV1(const xvalue*,xstrview,bool,xvalue**,bool*);
XL6_TEXT_API bool xl6DictErasedKeyV1(const xvalue*,xstrview,bool,bool*);
XL6_TEXT_API bool xl6DictErasedCountV1(const xvalue*,bool,int64_t*);
XL6_TEXT_API bool xl6DictErasedClearV1(const xvalue*);
XL6_TEXT_API bool xl6DictErasedReserveV1(const xvalue*,int64_t);
XL6_TEXT_API bool xl6DictErasedTrimV1(const xvalue*);
XL6_TEXT_API xvalue* xl6DictErasedCloneV1(const xvalue*);
/* Independent forward/reverse typed snapshot, including immutable admission
 * and codec. Status 0 only means unrelated; authentic native failures are -1.
 * All outputs private; owned cursor starts NULL. */
XL6_TEXT_API int xl6DictIteratorSnapshotErasedV1(const xvalue*,bool,xvalue**,XL6_DictLayoutV1*,uint64_t*);
/* One step publishes an owned Value AND independently owned exact text key,
 * not a borrowed source/cursor address. Outputs start NULL/{0} and are disjoint.
 * EOF publishes false/NULL/{0}. An advance/projection/copy failure is sticky;
 * rejected arguments do not advance. Free key.Data with xrtFree. This is not
 * the borrowed-key ValueIterator extension ABI; that integration is separate. */
XL6_TEXT_API bool xl6DictIteratorAdvanceValueV1(const xvalue*,const XL6_DictLayoutV1*,uint64_t,
    bool*,xvalue**,xstrview*);
/* Explicit ONE-level XVALUE_OBJECT snapshot, not var erasure or recursive
 * persistence. References remain actual references, keys preserve empty/NUL.
 * Nested native children require a closed graph/persistence provider. */
XL6_TEXT_API xvalue* xl6DictToWireV1(const xvalue*);
/* Consume private empty projected seed on BOTH paths. Exact complete domain
 * + identical resident codec uses atomic typed merge; other native domains
 * explicitly project one level before destination admission. Only wire OBJECT
 * accepted. Source untouched; aliases/nonempty/unbound seeds fail closed. */
XL6_TEXT_API xvalue* xl6DictImportTakeV1(xvalue*,const xvalue*);

/* Real slot ownership, including duplicate/self edges. Future slots require
 * independent authorization of their resident TraceSlot policy. Clear merely
 * detaches storage while frozen; checked Finish retires outside exclusion,
 * retaining detached storage for retry if its error boundary cannot start.
 * Structural Hash/Equal and source-language cutover are NOT supplied here.
 * They fail closed rather than inventing value semantics. */
XL6_TEXT_API const xrtownershipadapterv1* xl6DictOwnershipAdapterV1(xrtownershipref);
XL6_TEXT_API const xrtownershipadapterv1* xl6DictOwnershipAdapterV2(xrtownershipref,
    const XL6_SequenceAdmissionOpsV2* const*,size_t);
#endif
