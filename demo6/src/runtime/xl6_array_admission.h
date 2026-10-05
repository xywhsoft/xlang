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
/* Numeric enums require exact nominal identity AND full backing integer
 * range. i8..u32 use widened i64; u64 uses u64. Optional descriptors must be
 * immutable resident I64/U64 profile getter results, not module callbacks.
 * Nonnullable element/boxed identity match; nullable identities differ.
 * Defaults are zero/absent. No generic primitive or tagged-enum admission. */
XL6_TEXT_API xvalue* xl6ArrayNumericEnumCreateV1(uint64_t, uint64_t,
    const xrttype*, unsigned int, bool, uint64_t);
/* Typed function slots own boxed callable Value references, NOT XL6_Closure*.
 * Producers/consumers must use the explicit closure-value codec. Exact base
 * signature TypeId and actual resident callable family are required; an
 * unbound callable or a same-tag foreign handle is refused. Only nullable
 * slots have a default. This policy owns scalar metadata, no module callback,
 * and does not independently certify a callback's ownership/retirement graph. */
XL6_TEXT_API xvalue* xl6ArrayCallableCreateV1(uint64_t, uint64_t, uint64_t, bool);
/* Actual xfuture* slots copy the native consumer reference. The owned policy
 * traces those control blocks, not temporary boxes, and erased writes require
 * the exact base Future identity and genuine runtime Future Value family.
 * Native slots are statically typed by IR; this is not a payload-type test of
 * arbitrary C pointers. Nonnullable growth is forbidden; nullable yields NULL.
 * No generated descriptor, callback or code address is retained. */
XL6_TEXT_API xvalue* xl6ArrayFutureCreateV1(uint64_t, uint64_t, uint64_t, bool);
/* Exact resident graph capability; the Future policy still needs independent
 * authorization by the caller's resolver, including its payload and waiters. */
XL6_TEXT_API const XL6_SequenceAdmissionOpsV2* xl6ArrayFutureAdmissionOpsV2Get(void);
#endif
