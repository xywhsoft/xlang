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
/* Nonnullable C ABI scalars/type-id: actual native-width builtin descriptor,
 * exact dynamic tag/range/identity, zero default. The admitted boxed identity
 * equals the element identity. A type-id projection requires explicit identity;
 * an unbound ordinary UInt is not a type value. No custom descriptors. */
XL6_TEXT_API xvalue* xl6ArrayCScalarCreateV1(uint64_t, uint64_t, const xrttype*, uint64_t);
/* Nested Array references retain the same child owner. Child type identity
 * is checked against a sealed, profiled native ARRAY, not just Value.TypeId.
 * Nonnullable element identity equals child identity; nullable identity is
 * separate. No recursive traversal, borrowed schema/descriptor or code lease.
 * Nonnullable resize growth is forbidden; nullable growth yields absent. */
XL6_TEXT_API xvalue* xl6ArraySequenceCreateV1(uint64_t, uint64_t, uint64_t, bool);
/* Tagged native C scalars: absent has no payload domain; present uses the
 * exact builtin width/kind, with explicit identity for type values. The
 * optional descriptor must be an immutable resident profile getter result.
 * Optional logical element identity differs from the nonnullable identity. */
XL6_TEXT_API xvalue* xl6ArrayNullableCScalarCreateV1(uint64_t, uint64_t,
    const xrttype*, const xrttype*, uint64_t);
#endif
