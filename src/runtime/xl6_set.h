#ifndef XL6_SET_H
#define XL6_SET_H
#include "xl6_sequence.h"

/* A genuine typed Set owner; value.identity is an independent immutable
 * admission-domain witness, never an Array masquerading as Set storage. */
typedef struct XL6_SetLayoutV1 {
    uint64_t identity;
    XL6_SequenceLayoutV1 value;
} XL6_SetLayoutV1;
XL6_TEXT_API xvalue* xl6SetCreateTakeV1(const XL6_SetLayoutV1*,xvalue*);
XL6_TEXT_API bool xl6SetAdmitsSchemaV1(const xvalue*,const XL6_SetLayoutV1*,const XL6_SequenceAdmissionV3*);
XL6_TEXT_API bool xl6SetSchemaCopyV1(const xvalue*,XL6_SetLayoutV1*,XL6_SequenceAdmissionV3*);
/* All outputs are private initialized result slots, not language byref.
 * checked success is independent of found/new; failure does not publish them.
 * Basic values use typed/content keys; mutable references use stable instance
 * identity, and Future references use control-block identity. Mutation does
 * not invalidate a key. This interface does not freeze shared objects.
 * Cleanup failure after a committed removal is reported, not rolled back. */
XL6_TEXT_API bool xl6SetAddV1(const xvalue*,const XL6_SetLayoutV1*,const void*,bool*);
XL6_TEXT_API bool xl6SetKeyV1(const xvalue*,const XL6_SetLayoutV1*,const void*,bool,bool*);
XL6_TEXT_API bool xl6SetGetV1(const xvalue*,const XL6_SetLayoutV1*,const void*,ptr,bool,bool*);
XL6_TEXT_API bool xl6SetClearV1(const xvalue*,const XL6_SetLayoutV1*);
XL6_TEXT_API bool xl6SetReserveV1(const xvalue*,const XL6_SetLayoutV1*,int64_t);
XL6_TEXT_API bool xl6SetTrimV1(const xvalue*,const XL6_SetLayoutV1*);
XL6_TEXT_API int64_t xl6SetCountV1(const xvalue*,const XL6_SetLayoutV1*,bool);
XL6_TEXT_API xvalue* xl6SetCloneV1(const xvalue*,const XL6_SetLayoutV1*);
/* Exact whole logical domains must match. Set equality/hash disregard stable
 * insertion order, unlike List identity. Hash is a process-local key hash,
 * not a persisted format or cryptographic digest. */
XL6_TEXT_API bool xl6SetEqualsV1(const xvalue*,const xvalue*,const XL6_SetLayoutV1*,bool*);
XL6_TEXT_API bool xl6SetHashV1(const xvalue*,const XL6_SetLayoutV1*,uint64_t*);
XL6_TEXT_API bool xl6SetMergeV1(const xvalue*,const xvalue*,const XL6_SetLayoutV1*);
typedef enum XL6_SetAlgebraV1 {
    XL6_SET_UNION_V1=1,XL6_SET_INTERSECTION_V1,XL6_SET_DIFFERENCE_V1,XL6_SET_SYMMETRIC_DIFFERENCE_V1
} XL6_SetAlgebraV1;
XL6_TEXT_API xvalue* xl6SetAlgebraV1(const xvalue*,const xvalue*,const XL6_SetLayoutV1*,XL6_SetAlgebraV1);
/* Independently owned snapshot cursor, with one initialized current slot.
 * checked failure is sticky; EOF is separate and never masks a failure. */
XL6_TEXT_API xvalue* xl6SetCursorCreateV1(const xvalue*,const XL6_SetLayoutV1*,uint64_t,bool);
XL6_TEXT_API bool xl6SetCursorAdvanceV1(const xvalue*,const XL6_SetLayoutV1*,uint64_t,bool*);
XL6_TEXT_API bool xl6SetCursorReadV1(const xvalue*,const XL6_SetLayoutV1*,uint64_t,ptr);
XL6_TEXT_API const xrtownershipadapterv1* xl6SetOwnershipAdapterV1(xrtownershipref);
XL6_TEXT_API const xrtownershipadapterv1* xl6SetOwnershipAdapterV2(xrtownershipref,
    const XL6_SequenceAdmissionOpsV2* const*,size_t);
/* Immutable process-resident slot codec, consumed owner on both paths.
 * Reference slots use the shared retained-reference codec, not deep clone.
 * NULL for other slots selects XRT's default codec. Never generated code. */
XL6_TEXT_API xvalue* xl6SetProjectionTakeV1(xvalue*,const XL6_SetLayoutV1*,const xvalueconverter*);
/* A family probe is NOT admission: 0 unrelated, -1 observation refused,
 * 1 genuine family. Every operation independently pins the private owner,
 * checks its bound projection and complete logical/physical value domain.
 * Foreign handles are never cast. There is no wire-Set fallback. */
XL6_TEXT_API int xl6SetErasedProbeV1(const xvalue*);
typedef enum XL6_SetKeyActionV1 { XL6_SET_ADD_V1=1,XL6_SET_HAS_V1,XL6_SET_REMOVE_V1 } XL6_SetKeyActionV1;
XL6_TEXT_API bool xl6SetErasedKeyV1(const xvalue*,const xvalue*,XL6_SetKeyActionV1,bool*);
XL6_TEXT_API bool xl6SetErasedReadV1(const xvalue*,const xvalue*,bool,xvalue**,bool*);
XL6_TEXT_API bool xl6SetErasedCountV1(const xvalue*,bool,int64_t*);
XL6_TEXT_API bool xl6SetErasedClearV1(const xvalue*);
XL6_TEXT_API bool xl6SetErasedReserveV1(const xvalue*,int64_t);
XL6_TEXT_API bool xl6SetErasedTrimV1(const xvalue*);
XL6_TEXT_API xvalue* xl6SetErasedCloneV1(const xvalue*);
XL6_TEXT_API bool xl6SetErasedHashV1(const xvalue*,uint64_t*);
typedef enum XL6_SetRelationV1 { XL6_SET_EQUALS_V1=1,XL6_SET_SUBSET_V1,XL6_SET_SUPERSET_V1,XL6_SET_DISJOINT_V1 } XL6_SetRelationV1;
XL6_TEXT_API bool xl6SetErasedRelationV1(const xvalue*,const xvalue*,XL6_SetRelationV1,bool,bool*);
XL6_TEXT_API bool xl6SetErasedMergeV1(const xvalue*,const xvalue*);
XL6_TEXT_API xvalue* xl6SetErasedAlgebraV1(const xvalue*,const xvalue*,XL6_SetAlgebraV1);
/* Snapshot and projected iteration outputs publish only on checked success.
 * Step combines canonical slot projection with the cursor's sticky failure. */
XL6_TEXT_API int xl6SetIteratorSnapshotErasedV1(const xvalue*,bool,xvalue**,XL6_SetLayoutV1*,uint64_t*);
XL6_TEXT_API bool xl6SetIteratorAdvanceValueV1(const xvalue*,const XL6_SetLayoutV1*,uint64_t,bool*,xvalue**);
#endif
