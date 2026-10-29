#ifndef XL6_SLOT_MAP_H
#define XL6_SLOT_MAP_H
#include "xl6_sequence.h"
#include "xl6_slot_map_key_abi.h"

/* Private typed SlotMap foundation. XRT owns the slot/free-list/generation
 * algorithm; these boundaries own elements and a sealed resident domain.
 * No generated descriptor/callback is retained. Language wiring is separate. */
typedef struct XL6_SlotMapLayoutV1 {
    uint64_t identity;
    XL6_SequenceLayoutV1 value;
} XL6_SlotMapLayoutV1;

/* Immutable owned key VALUE, not a borrow or a raw address. The small instance
 * record has no edge to the map, code or any element. A key can outlive its map
 * without keeping it alive; its record prevents allocator/DLL reload ABA.
 * Native callers only pass genuine keys; instances cannot be fabricated.
 * Keys are not serializable process-independent identifiers. */
XL6_TEXT_API const xrttype* xl6SlotMapKeyTypeV1(void);
XL6_TEXT_API bool xl6SlotMapKeyEqualV1(XL6_SlotMapKeyV1,XL6_SlotMapKeyV1);
XL6_TEXT_API void xl6SlotMapKeyDropValueV1(XL6_SlotMapKeyV1);
XL6_TEXT_API bool xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1,XL6_SlotMapKeyV1*);
/* Consume a fresh empty admitted ARRAY witness on BOTH paths. */
XL6_TEXT_API xvalue* xl6SlotMapCreateTakeV1(const XL6_SlotMapLayoutV1*,xvalue*);
/* Error-transparent immutable full-domain probe. Private owner ops are
 * checked before dereferencing foreign pointers; BUSY is allowed. No key
 * instance or mutable slot is read, and no source reference is acquired. */
XL6_TEXT_API bool xl6SlotMapAdmitsSchemaV1(const xvalue*,const XL6_SlotMapLayoutV1*,const XL6_SequenceAdmissionV3*);
/* Key output starts {0}; insertion copies the exact typed input. No value
 * is published before all allocation/admission succeeds. One native owned
 * allocation per element follows XRT slot_map's nonempty pointer storage. */
XL6_TEXT_API bool xl6SlotMapInsertV1(const xvalue*,const XL6_SlotMapLayoutV1*,const void*,XL6_SlotMapKeyV1*);
/* Stale/zero/other-instance keys mean normal absence. Checked success is
 * separate from found/updated. Result output is a private initialized EMPTY
 * typed slot; read copies, never lends a source address. */
XL6_TEXT_API bool xl6SlotMapReadV1(const xvalue*,const XL6_SlotMapLayoutV1*,XL6_SlotMapKeyV1,void*,bool*);
/* Language get: normal absence is a checked range error, not an empty T. */
XL6_TEXT_API bool xl6SlotMapReadRequiredV1(const xvalue*,const XL6_SlotMapLayoutV1*,XL6_SlotMapKeyV1,void*);
XL6_TEXT_API bool xl6SlotMapSetV1(const xvalue*,const XL6_SlotMapLayoutV1*,XL6_SlotMapKeyV1,const void*,bool*);
XL6_TEXT_API bool xl6SlotMapKeyV1(const xvalue*,const XL6_SlotMapLayoutV1*,XL6_SlotMapKeyV1,bool,bool*);
XL6_TEXT_API bool xl6SlotMapClearV1(const xvalue*,const XL6_SlotMapLayoutV1*);
XL6_TEXT_API bool xl6SlotMapReserveV1(const xvalue*,const XL6_SlotMapLayoutV1*,int64_t);
XL6_TEXT_API int64_t xl6SlotMapCountV1(const xvalue*,const XL6_SlotMapLayoutV1*);
XL6_TEXT_API int64_t xl6SlotMapCapacityV1(const xvalue*,const XL6_SlotMapLayoutV1*);
/* Checked language size query: diagnostic allocation failure is still false. */
XL6_TEXT_API bool xl6SlotMapSizeV1(const xvalue*,const XL6_SlotMapLayoutV1*,bool,int64_t*);
/* Independent native ARRAY snapshots. Values keep the exact captured domain;
 * keys own instance records, not the source map. Converter is immutable
 * resident metadata, validated by the shared Sequence projection boundary.
 * Sparse slots are traversed in supplier slot order, not insertion order. */
XL6_TEXT_API xvalue* xl6SlotMapValuesV1(const xvalue*,const XL6_SlotMapLayoutV1*,const xvalueconverter*);
XL6_TEXT_API xvalue* xl6SlotMapKeysV1(const xvalue*,const XL6_SlotMapLayoutV1*,uint64_t);
/* Resident-only shared snapshot transaction. A key schema witness is consumed
 * on BOTH paths, but never mutated: its aliases cannot observe partial output.
 * The value lane needs no witness and uses the map's captured immutable policy.
 * No raw slot address, generated callback or mutable source borrow escapes. */
XL6_TEXT_API xvalue* xl6SlotMapSnapshotTakeV1(const xvalue*,const XL6_SlotMapLayoutV1*,
    bool,xvalue*,const xvalueconverter*);
/* One source pin and one supplier traversal produce aligned independent
 * snapshots. The private output starts empty and is unchanged on failure.
 * Take consumes the empty key witness on BOTH paths, without mutating its
 * aliases. Success transfers both owners together; neither owns the source.
 * This is a resident foundation, not two calls to keys()/values(). */
typedef struct XL6_SlotMapSnapshotPairV1 { xvalue* keys; xvalue* values; } XL6_SlotMapSnapshotPairV1;
XL6_TEXT_API bool xl6SlotMapSnapshotPairTakeV1(const xvalue*,const XL6_SlotMapLayoutV1*,
    xvalue*,const xvalueconverter*,const xvalueconverter*,XL6_SlotMapSnapshotPairV1*);
/* Independent optional producer: only paired consumers select key boxing,
 * key schema construction and this provider. No public language API yet. */
XL6_TEXT_API bool xl6SlotMapPairsV1(const xvalue*,const XL6_SlotMapLayoutV1*,
    uint64_t,const xvalueconverter*,XL6_SlotMapSnapshotPairV1*);
/* Clear/remove/replace commit before old-element cleanup; a cleanup failure
 * is checked and must not be blindly retried as an uncommitted operation.
 * Full real edges and phased retirement, including duplicate/self edges.
 * FUTURE_REF requires independent authorization of its resident TraceSlot. */
XL6_TEXT_API const xrtownershipadapterv1* xl6SlotMapOwnershipAdapterV1(xrtownershipref);
XL6_TEXT_API const xrtownershipadapterv1* xl6SlotMapOwnershipAdapterV2(xrtownershipref,
    const XL6_SequenceAdmissionOpsV2* const*,size_t);
#endif
