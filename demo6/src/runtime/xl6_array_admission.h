#ifndef XL6_ARRAY_ADMISSION_H
#define XL6_ARRAY_ADMISSION_H
#include "xl6_sequence.h"
#include "xl6_value_admission_abi.h"
#include "xl6_nominal_admission_abi.h"
/* Shared ARRAY/STACK factories; the first argument is the exact native role.
 * Profiled native reference storage. Metadata is copied/owned, not borrowed
 * from generated code. Only ANY/bytes use the primitive reference carrier;
 * scalar and text physical descriptors require their separate slot codecs. */
/* The owner element identity may name nullable<T>; the final identity names
 * T's admitted boxed value domain. They must not be conflated by the backend. */
XL6_TEXT_API xvalue* xl6SequenceReferenceCreateV1(unsigned int, uint64_t, uint64_t, unsigned int, bool, uint64_t);
/* Exact canonical ANY reference policy on an operation-pinned owner. No
 * allocation or storage reads. The caller must independently verify role,
 * owner identity and the published resident reference projection. */
XL6_TEXT_API bool xl6SequenceReferenceAnyPolicyV1(XL6_Sequence*, uint64_t);
/* Immutable resident-policy probe shared by distinct Array/List owners.
 * No allocation, slot access, borrowed policy retention or error changes. */
XL6_TEXT_API bool xl6ReferenceAnyAdmissionV1(const XL6_SequenceAdmissionV3*,uint64_t);
XL6_TEXT_API bool xl6ReferenceBytesAdmissionV1(const XL6_SequenceAdmissionV3*,uint64_t);
XL6_TEXT_API bool xl6StringArrayAdmissionV1(const XL6_SequenceAdmissionV3*,uint64_t);
XL6_TEXT_API xvalue* xl6SequenceNominalCreateV1(unsigned int, uint64_t, uint64_t, const XL6_NominalAdmissionV1*, bool, uint64_t);
/* Untagged trivial union object representations, exact nominal identity and
 * sizeof(C union). Physical slots own boxed bytes, not raw union pointers.
 * Zero growth is materialized by resident code; no generated callback lives
 * in this independently owned schema. This is not an active-variant codec. */
XL6_TEXT_API xvalue* xl6SequenceUnionCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, size_t, bool);
/* Exact logical scalar/text policy on the selected physical slot descriptor.
 * Integer widths use int64 storage (including u8/u16/u32), not coercion rules.
 * nullable element identity and nonnullable boxed value identity stay distinct.
 * Descriptors/converters are immutable resident C metadata, never callbacks
 * into generated code. No optional physical factory is selected implicitly. */
XL6_TEXT_API xvalue* xl6SequencePrimitiveCreateV1(unsigned int, uint64_t, uint64_t, unsigned int, size_t,
    const xrttype*, unsigned int, bool, uint64_t);
/* Nonnullable C ABI scalars/type-id: actual native-width builtin descriptor,
 * exact dynamic tag/range/identity, zero default. The admitted boxed identity
 * equals the element identity. A type-id projection requires explicit identity;
 * an unbound ordinary UInt is not a type value. No custom descriptors. */
XL6_TEXT_API xvalue* xl6SequenceCScalarCreateV1(unsigned int, uint64_t, uint64_t, const xrttype*, uint64_t);
/* The child schema exemplar is consumed on BOTH paths. Only its immutable
 * role/layout/full admission metadata is retained, not the exemplar, its
 * storage or its elements. Clones/cursors own the entire recursive schema,
 * independently of source modules/wrappers. No layout-only nested admission.
 * The chain is bounded at 64; slot checks do not traverse child contents.
 * Nonnullable element identity equals child identity; nullable is separate.
 * Nonnullable resize growth is forbidden; nullable growth yields absent. */
XL6_TEXT_API xvalue* xl6SequenceNestedCreateTakeV2(unsigned int, uint64_t, uint64_t, xvalue*, bool);
/* Explicit import-plan edge validation. Parent is operation-pinned; child is
 * a sealed schema witness (BUSY is legal). Only the actual nested resident
 * policy authorizes this edge; IDs/layout equality alone are insufficient.
 * NULL checks a chain endpoint: no Array child may remain unplanned. A Stack
 * child ends data conversion and must already be a genuine native Stack. */
XL6_TEXT_API bool xl6SequenceNestedChildCheckV1(XL6_Sequence*, const xvalue*);
/* Tagged native C scalars: absent has no payload domain; present uses the
 * exact builtin width/kind, with explicit identity for type values. The
 * optional descriptor must be an immutable resident profile getter result.
 * Optional logical element identity differs from the nonnullable identity. */
XL6_TEXT_API xvalue* xl6SequenceNullableCScalarCreateV1(unsigned int, uint64_t, uint64_t,
    const xrttype*, const xrttype*, uint64_t);
/* Numeric enums require exact nominal identity AND full backing integer
 * range. i8..u32 use widened i64; u64 uses u64. Optional descriptors must be
 * immutable resident I64/U64 profile getter results, not module callbacks.
 * Nonnullable element/boxed identity match; nullable identities differ.
 * Defaults are zero/absent. No generic primitive or tagged-enum admission. */
XL6_TEXT_API xvalue* xl6SequenceNumericEnumCreateV1(unsigned int, uint64_t, uint64_t,
    const xrttype*, unsigned int, bool, uint64_t);
/* Typed function slots own boxed callable Value references, NOT XL6_Closure*.
 * Producers/consumers must use the explicit closure-value codec. Exact base
 * signature TypeId and actual resident callable family are required; an
 * unbound callable or a same-tag foreign handle is refused. Only nullable
 * slots have a default. This policy owns scalar metadata, no module callback,
 * and does not independently certify a callback's ownership/retirement graph. */
/* Error slots own genuine private error Value handles. Their logical element
 * identity is preserved by the explicit error-value codec; nullable slots
 * alone admit null and have a default. No generated callback is retained. */
XL6_TEXT_API xvalue* xl6SequenceErrorCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, bool);
XL6_TEXT_API xvalue* xl6SequenceCallableCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, bool);
/* Exact resident, nonnullable callable slot schema on an operation pin.
 * Does not certify the callback signature shape or its ownership graph. */
XL6_TEXT_API bool xl6SequenceCallablePolicyV1(XL6_Sequence*,uint64_t);
/* Actual xfuture* slots copy the native consumer reference. The owned policy
 * traces those control blocks, not temporary boxes, and erased writes require
 * the exact base Future identity and genuine runtime Future Value family.
 * Native slots are statically typed by IR; this is not a payload-type test of
 * arbitrary C pointers. Nonnullable growth is forbidden; nullable yields NULL.
 * No generated descriptor, callback or code address is retained. */
XL6_TEXT_API xvalue* xl6SequenceFutureCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, bool);
/* Exact resident graph capability; the Future policy still needs independent
 * authorization by the caller's resolver, including its payload and waiters. */
XL6_TEXT_API const XL6_SequenceAdmissionOpsV2* xl6ArrayFutureAdmissionOpsV2Get(void);
#endif
