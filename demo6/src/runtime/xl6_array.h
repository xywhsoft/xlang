#ifndef XL6_ARRAY_H
#define XL6_ARRAY_H
#include "src/runtime/xl6_sequence.h"

/* Compiler-private typed Array ABI. Empty result slots belong to the callee
 * expression, never to a language byref parameter. The verified tuple names
 * the precise container/element identities and physical resident descriptor. */
#define XL6_ARRAY_LAYOUT uint64_t, uint64_t, unsigned int, size_t, const xrttype*
XL6_TEXT_API xvalue* xl6ArrayCreateV1(XL6_ARRAY_LAYOUT);
XL6_TEXT_API bool xl6ArrayPushV1(const xvalue*, XL6_ARRAY_LAYOUT, const void*);
XL6_TEXT_API bool xl6ArraySetV1(const xvalue*, XL6_ARRAY_LAYOUT, int64_t, const void*);
XL6_TEXT_API bool xl6ArrayInsertV1(const xvalue*, XL6_ARRAY_LAYOUT, int64_t, const void*);
XL6_TEXT_API void xl6ArrayReadV1(const xvalue*, XL6_ARRAY_LAYOUT, int64_t, void*);
XL6_TEXT_API void xl6ArrayTakeV1(const xvalue*, XL6_ARRAY_LAYOUT, int64_t, void*);
XL6_TEXT_API bool xl6ArrayRemoveV1(const xvalue*, XL6_ARRAY_LAYOUT, int64_t, int64_t);
XL6_TEXT_API bool xl6ArrayReserveV1(const xvalue*, XL6_ARRAY_LAYOUT, int64_t);
XL6_TEXT_API bool xl6ArrayResizeV1(const xvalue*, XL6_ARRAY_LAYOUT, int64_t);
XL6_TEXT_API bool xl6ArrayTrimV1(const xvalue*, XL6_ARRAY_LAYOUT);
XL6_TEXT_API void xl6ArrayClearV1(const xvalue*, XL6_ARRAY_LAYOUT);
XL6_TEXT_API int64_t xl6ArraySizeV1(const xvalue*, XL6_ARRAY_LAYOUT, bool);
XL6_TEXT_API xvalue* xl6ArrayCloneV1(const xvalue*, XL6_ARRAY_LAYOUT);
XL6_TEXT_API bool xl6ArrayEqualsV1(const xvalue*, const xvalue*, XL6_ARRAY_LAYOUT);
XL6_TEXT_API bool xl6ArrayAppendV1(const xvalue*, const xvalue*, XL6_ARRAY_LAYOUT);
XL6_TEXT_API bool xl6ArraySwapV1(const xvalue*, XL6_ARRAY_LAYOUT, int64_t, int64_t);
XL6_TEXT_API bool xl6ArrayReverseV1(const xvalue*, XL6_ARRAY_LAYOUT);
/* First equal element, or -1 when absent. Errors remain exceptional. */
XL6_TEXT_API int64_t xl6ArrayFindV1(const xvalue*, XL6_ARRAY_LAYOUT, const void*);
#undef XL6_ARRAY_LAYOUT
#endif
