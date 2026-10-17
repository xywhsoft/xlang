#ifndef XL6_VALUE_ADMISSION_H
#define XL6_VALUE_ADMISSION_H
#include "xl6_value_admission_abi.h"
#include "xl6_text_value.h"

/* Allocation-free exact-domain predicate. No coercion, no mutation and no
 * error-context changes, including mismatch/invalid arguments. */
XL6_TEXT_API bool xl6ValueAdmitsV1(const xvalue*, unsigned int, bool, uint64_t);
XL6_TEXT_API xvalue* xl6ValueCheckedRetainV1(const xvalue*, unsigned int, bool, uint64_t);
/* Non-throwing nullable bytes projection. Mismatch returns the immutable null
 * singleton; success owns one reference. No allocation or error mutation. */
XL6_TEXT_API xvalue* xl6ValueTryBytesV1(const xvalue*, uint64_t);
/* No-raise nullable integer projection, with the same exact domain/range
 * admission as typed container reads. Mismatch leaves the output untouched. */
XL6_TEXT_API bool xl6ValueTryReadIntegerV1(const xvalue*, unsigned int, uint64_t, void*);
/* Status/output contract; a failed projection leaves output untouched. */
XL6_TEXT_API bool xl6ValueReadF32V1(const xvalue*, uint64_t, float*);
/* Checked operation status is independent of ordinary type mismatch. Present
 * is committed on success; an absent result leaves payload unchanged. No
 * explicit rounding/conversion is performed and logical identity is exact. */
XL6_TEXT_API bool xl6ValueTryReadF32V1(const xvalue*,uint64_t,bool*,float*);
/* I8..U32 use an int64_t physical slot; U64 uses uint64_t. The rule fixes the
 * complete logical range, not the pointed-to language spelling. Slot admission
 * is pure and allocation-free; read failure never changes the output. A nominal
 * input must carry identity, and boxing binds it only to a newly owned value. */
XL6_TEXT_API bool xl6ValueIntegerSlotAdmitsV1(const void*, unsigned int);
XL6_TEXT_API bool xl6ValueReadIntegerV1(const xvalue*, unsigned int, uint64_t, bool, void*);
XL6_TEXT_API xvalue* xl6ValueBoxIntegerV1(const void*, unsigned int, uint64_t, bool);
/* Consumes one source owner on BOTH paths. Success preserves its content and
 * shell; failure preserves the validation error across destructive cleanup.
 * NONE/false requests only the container kind check. */
XL6_TEXT_API xvalue* xl6CollectionCheckedTakeV1(xvalue*, int, unsigned int, bool, uint64_t);
#endif
