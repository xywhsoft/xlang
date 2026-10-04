#ifndef XL6_ARRAY_ADMISSION_H
#define XL6_ARRAY_ADMISSION_H
#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_value_admission_abi.h"
#include "src/runtime/xl6_nominal_admission_abi.h"
/* Profiled native reference storage. Metadata is copied/owned, not borrowed
 * from generated code. Only ANY/bytes use the primitive reference carrier;
 * scalar and text physical descriptors require their separate slot codecs. */
/* The owner element identity may name nullable<T>; the final identity names
 * T's admitted boxed value domain. They must not be conflated by the backend. */
XL6_TEXT_API xvalue* xl6ArrayReferenceCreateV2(uint64_t, uint64_t, unsigned int, bool, uint64_t);
XL6_TEXT_API xvalue* xl6ArrayNominalCreateV2(uint64_t, uint64_t, const XL6_NominalAdmissionV1*, bool, uint64_t);
/* Exact logical scalar/text policy on the selected physical slot descriptor.
 * Integer widths use int64 storage (including u8/u16/u32), not coercion rules.
 * nullable element identity and nonnullable boxed value identity stay distinct.
 * Descriptors/converters are immutable resident C metadata, never callbacks
 * into generated code. No optional physical factory is selected implicitly. */
XL6_TEXT_API xvalue* xl6ArrayPrimitiveCreateV2(uint64_t, uint64_t, unsigned int, size_t,
    const xrttype*, unsigned int, bool, uint64_t);
#endif
