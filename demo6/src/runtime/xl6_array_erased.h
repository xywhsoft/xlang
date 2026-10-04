#ifndef XL6_ARRAY_ERASED_H
#define XL6_ARRAY_ERASED_H
#include "src/runtime/xl6_sequence.h"

/* Explicit immutable resident projection policy. Retains the SAME shell and
 * owner without allocation, binding once; no snapshot/COW/container clone.
 * NULL uses XRT's canonical builtin conversions; transfer 3 uses retain.
 * Dynamic operations reject non-ARRAY, unbound and BUSY carriers. Read/Take
 * return owned values; conversion failure preserves the original storage. */
XL6_TEXT_API xvalue* xl6ArrayEraseV1(const xvalue*, uint64_t, uint64_t, unsigned int,
    size_t, const xrttype*, const xvalueconverter*);
XL6_TEXT_API xvalue* xl6ArrayErasedReadV1(const xvalue*, int64_t);
XL6_TEXT_API xvalue* xl6ArrayErasedTakeV1(const xvalue*, int64_t);
XL6_TEXT_API bool xl6ArrayErasedSetV1(const xvalue*, int64_t, const xvalue*);
XL6_TEXT_API bool xl6ArrayErasedPushV1(const xvalue*, const xvalue*);
XL6_TEXT_API bool xl6ArrayErasedInsertV1(const xvalue*, int64_t, const xvalue*);
XL6_TEXT_API int64_t xl6ArrayErasedSizeV1(const xvalue*);
XL6_TEXT_API xvalue* xl6ArrayErasedCloneV1(const xvalue*);
XL6_TEXT_API void xl6ArrayErasedClearV1(const xvalue*);
#endif
