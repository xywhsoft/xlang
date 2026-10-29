#ifndef XL6_DICT_VALUE_INDEX_H
#define XL6_DICT_VALUE_INDEX_H
#include "xl6_text_value.h"
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_DICT_VALUE_INDEX_API __declspec(dllexport)
#else
#define XL6_DICT_VALUE_INDEX_API
#endif
/* Borrowed receiver and exact byte-span key; independent owned Value result.
 * Native owners must pass their genuine full-domain/codec authentication.
 * XVALUE_OBJECT is an explicit legacy Dict carrier until source factories
 * migrate, not a fallback for invalid/BUSY/unbound/native cursor owners.
 * A missing key returns canonical null, or RANGE for Required. Only OrNull
 * treats a NULL/canonical-null receiver as a normal miss. Present null is a
 * value, not a missing key. Failures latch the enclosing checked outcome,
 * including when allocation of the diagnostic itself fails. */
XL6_DICT_VALUE_INDEX_API xvalue* xl6DictValueReadCopyV1(const xvalue*,xstrview);
XL6_DICT_VALUE_INDEX_API xvalue* xl6DictValueReadOrNullCopyV1(const xvalue*,xstrview);
XL6_DICT_VALUE_INDEX_API xvalue* xl6DictValueReadRequiredCopyV1(const xvalue*,xstrview);
/* Consume item on BOTH paths, even if entry to the error scope fails. Borrow
 * key/receiver only. Wire replacement retires the old child outside its BUSY
 * fence; native replacement follows its authenticated typed storage contract.
 * The first failure is preserved through all cleanup callbacks. */
XL6_DICT_VALUE_INDEX_API bool xl6DictValueSetTakeV1(xvalue*,xstrview,xvalue*);
#endif
