#ifndef XL6_NATIVE_SCALAR_VALUE_H
#define XL6_NATIVE_SCALAR_VALUE_H
#include "xl6_native_scalar_abi.h"
#include "xl6_text_value.h"
/* Exact domain, no numeric coercion. Read commits native bytes only after a
 * successful conversion, including unaligned output slots. Box binds identity
 * only on an independently owned primitive, never the canonical bool singleton.
 * TYPE_ID requires its exact logical identity on input; other primitives may
 * be unbound but must not carry a foreign nominal identity. */
XL6_TEXT_API bool xl6ValueReadNativeScalarV1(const xvalue*, unsigned int, uint64_t, void*);
/* Pure overload admission with target C widths; no coercion/allocation/error
 * changes. Nullable admits canonical null only for a valid profile/identity. */
XL6_TEXT_API bool xl6ValueAdmitsNativeScalarV1(const xvalue*,unsigned int,uint64_t,bool);
XL6_TEXT_API xvalue* xl6ValueBoxNativeScalarV1(const void*, unsigned int, uint64_t);
/* Safe explicit numeric conversion, distinct from lossless typed reads.
 * Input/output may be unaligned; output commits only on checked success. */
XL6_TEXT_API bool xl6NativeScalarConvertV1(const void*,unsigned int,unsigned int,void*);
/* Reflection identity is supplied by the verified language type map. It must
 * be nonzero; reflection UInt carriers never enter numeric conversion. */
XL6_TEXT_API bool xl6ValueConvertNativeScalarV2(const xvalue*,unsigned int,uint64_t,void*);
/* A mismatch/null is absent; actual checked failures are not swallowed. */
XL6_TEXT_API bool xl6ValueTryReadNativeScalarV1(const xvalue*,unsigned int,uint64_t,bool*,void*);
#endif
