#ifndef XL6_ARRAY_EXTRACT_H
#define XL6_ARRAY_EXTRACT_H
#include "xl6_text_value.h"

/* Borrow receiver. Array Take uses a nonnegative element index; Pop selects
 * the last element and requires index=0. Missing/invalid Array items raise.
 * Recognized native owners must be bound, idle and match both identities.
 * The resident slot policy admits the stored element before any projection.
 * Scalar native slots move without boxing. String native slots move their
 * owned view; wire strings prepare an exact-length copy before removal.
 * Value projection is prepared and admitted before a native item is removed.
 * Failure returns a zero/NULL carrier with an error; false, zero and absent
 * nullable text can also be valid successes. Array has no ordinary miss.
 * Allocation/conversion failure before commit preserves the collection.
 * Cleanup after a committed removal may fail without undoing that removal. */
XL6_TEXT_API bool xl6ArrayExtractBoolV1(const xvalue*,uint64_t,uint64_t,int64_t,bool);
XL6_TEXT_API int64_t xl6ArrayExtractI64V1(const xvalue*,uint64_t,uint64_t,int64_t,bool);
XL6_TEXT_API uint64_t xl6ArrayExtractU64V1(const xvalue*,uint64_t,uint64_t,int64_t,bool);
XL6_TEXT_API double xl6ArrayExtractF64V1(const xvalue*,uint64_t,uint64_t,int64_t,bool);
XL6_TEXT_API float xl6ArrayExtractF32V1(const xvalue*,uint64_t,uint64_t,int64_t,bool);
XL6_TEXT_API int64_t xl6ArrayExtractTimeV1(const xvalue*,uint64_t,uint64_t,int64_t,bool);
XL6_TEXT_API uint32_t xl6ArrayExtractCharV1(const xvalue*,uint64_t,uint64_t,int64_t,bool);
/* nullable selects the verified language result contract, not a source cast.
 * An absent nullable string remains {NULL,0}, distinct from a present empty
 * string. On failure any prepared owner is released before returning. */
XL6_TEXT_API xstrview xl6ArrayExtractStringV1(const xvalue*,uint64_t,uint64_t,int64_t,bool,bool);
XL6_TEXT_API xvalue* xl6ArrayExtractValueV1(const xvalue*,uint64_t,uint64_t,int64_t,bool);
/* Closed trivial scalar/nullable descriptors only. Exact logical admission
 * precedes wire conversion/removal. Native slots move without projection.
 * Output may be unaligned and is unchanged on failure; null presence is data.
 * rule=0 selects the exact builtin C domain, otherwise a language value domain.
 * The descriptor/converter are resident, not generated module callbacks. */
XL6_TEXT_API bool xl6ArrayExtractTrivialV1(const xvalue*,uint64_t,uint64_t,int64_t,bool,
    const xrttype*,const xrttype*,const xvalueconverter*,unsigned int,uint64_t,void*);
/* List/dictionary Take returns an owned canonical null on an ordinary miss.
 * Their explicit carrier codes are independent of the compiler enum. */
#define XL6_COLLECTION_EXTRACT_LIST_V1 2u
#define XL6_COLLECTION_EXTRACT_DICT_V1 4u
XL6_TEXT_API xvalue* xl6CollectionExtractValueV1(const xvalue*,unsigned int,int64_t,xstrview);
#endif
