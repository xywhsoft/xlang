#ifndef XL6_NOMINAL_ADMISSION_H
#define XL6_NOMINAL_ADMISSION_H
#include "src/runtime/xl6_nominal_admission_abi.h"
#include "src/runtime/xl6_text_value.h"

/* Allocation-free/error-transparent even on mismatch or invalid arguments.
 * Schema lifetimes are limited to this call; no module callbacks are stored. */
XL6_TEXT_API bool xl6ValueAdmitsNominalV1(const xvalue*, const XL6_NominalAdmissionV1*, bool);
/* Consumes one source owner on both paths. NULL is not a language null value.
 * Success preserves shell/content; cleanup preserves the original failure. */
XL6_TEXT_API xvalue* xl6CollectionCheckedNominalTakeV1(xvalue*, int, const XL6_NominalAdmissionV1*, bool);
#endif
