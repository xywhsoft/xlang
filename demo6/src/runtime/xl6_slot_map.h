#ifndef XL6_SLOT_MAP_H
#define XL6_SLOT_MAP_H
#include "xl6_sequence.h"

/* Private typed SlotMap foundation. XRT owns the slot/free-list/generation
 * algorithm; these boundaries own elements and a sealed resident domain.
 * No generated descriptor/callback is retained. Language wiring is separate. */
typedef struct XL6_SlotMapLayoutV1 {
    uint64_t identity;
    XL6_SequenceLayoutV1 value;
} XL6_SlotMapLayoutV1;
typedef struct XL6_SlotMapInstanceV1 XL6_SlotMapInstanceV1;
typedef struct XL6_SlotMapKeyV1 {
    XL6_SlotMapInstanceV1* instance;
    xslot slot;
} XL6_SlotMapKeyV1;

/* Immutable owned key VALUE, not a borrow or a raw address. The small instance
 * record has no edge to the map, code or any element. A key can outlive its map
 * without keeping it alive; its record prevents allocator/DLL reload ABA.
 * Native callers only pass genuine keys; instances cannot be fabricated.
 * Keys are not serializable process-independent identifiers. */
XL6_TEXT_API const xrttype* xl6SlotMapKeyTypeV1(void);
XL6_TEXT_API bool xl6SlotMapKeyEqualV1(XL6_SlotMapKeyV1,XL6_SlotMapKeyV1);
/* Consume a fresh empty admitted ARRAY witness on BOTH paths. */
XL6_TEXT_API xvalue* xl6SlotMapCreateTakeV1(const XL6_SlotMapLayoutV1*,xvalue*);
/* Key output starts {0}; insertion copies the exact typed input. No value
 * is published before all allocation/admission succeeds. One native owned
 * allocation per element follows XRT slot_map's nonempty pointer storage. */
XL6_TEXT_API bool xl6SlotMapInsertV1(const xvalue*,const XL6_SlotMapLayoutV1*,const void*,XL6_SlotMapKeyV1*);
/* Stale/zero/other-instance keys mean normal absence. Checked success is
 * separate from found/updated. Result output is a private initialized EMPTY
 * typed slot; read copies, never lends a source address. */
XL6_TEXT_API bool xl6SlotMapReadV1(const xvalue*,const XL6_SlotMapLayoutV1*,XL6_SlotMapKeyV1,void*,bool*);
XL6_TEXT_API bool xl6SlotMapSetV1(const xvalue*,const XL6_SlotMapLayoutV1*,XL6_SlotMapKeyV1,const void*,bool*);
XL6_TEXT_API bool xl6SlotMapKeyV1(const xvalue*,const XL6_SlotMapLayoutV1*,XL6_SlotMapKeyV1,bool,bool*);
XL6_TEXT_API bool xl6SlotMapClearV1(const xvalue*,const XL6_SlotMapLayoutV1*);
XL6_TEXT_API bool xl6SlotMapReserveV1(const xvalue*,const XL6_SlotMapLayoutV1*,int64_t);
XL6_TEXT_API int64_t xl6SlotMapCountV1(const xvalue*,const XL6_SlotMapLayoutV1*);
XL6_TEXT_API int64_t xl6SlotMapCapacityV1(const xvalue*,const XL6_SlotMapLayoutV1*);
/* Clear/remove/replace commit before old-element cleanup; a cleanup failure
 * is checked and must not be blindly retried as an uncommitted operation.
 * Full real edges and phased retirement, including duplicate/self edges.
 * FUTURE_REF requires independent authorization of its resident TraceSlot. */
XL6_TEXT_API const xrtownershipadapterv1* xl6SlotMapOwnershipAdapterV1(xrtownershipref);
XL6_TEXT_API const xrtownershipadapterv1* xl6SlotMapOwnershipAdapterV2(xrtownershipref,
    const XL6_SequenceAdmissionOpsV2* const*,size_t);
#endif
