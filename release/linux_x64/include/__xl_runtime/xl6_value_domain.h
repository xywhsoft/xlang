#ifndef XL6_VALUE_DOMAIN_H
#define XL6_VALUE_DOMAIN_H
#include "xl6_value_admission_abi.h"
#include <stdint.h>
#include <stdbool.h>
#include <float.h>
#include <limits.h>
#include <math.h>

/* Logical primitive domains, shared by typed slots and boxed admission.
 * These are pure predicates: no allocations, conversions or error changes. */
static inline bool xl6IntegerDomainV1(int64_t number, unsigned int rule)
{
    switch (rule) {
    case XL6_VALUE_ADMIT_I8: return number>=INT8_MIN && number<=INT8_MAX;
    case XL6_VALUE_ADMIT_I16: return number>=INT16_MIN && number<=INT16_MAX;
    case XL6_VALUE_ADMIT_I32: return number>=INT32_MIN && number<=INT32_MAX;
    case XL6_VALUE_ADMIT_I64: return true;
    case XL6_VALUE_ADMIT_U8: return number>=0 && number<=UINT8_MAX;
    case XL6_VALUE_ADMIT_U16: return number>=0 && number<=UINT16_MAX;
    case XL6_VALUE_ADMIT_U32: return number>=0 && number<=UINT32_MAX;
    default: return false;
    }
}
static inline bool xl6Float32DomainV1(double number)
{
    return isnan(number) || isinf(number) ||
        (number>=-FLT_MAX && number<=FLT_MAX && (double)(float)number==number);
}
static inline bool xl6CharDomainV1(uint32_t scalar)
{
    return scalar<=UINT32_C(0x10FFFF) &&
        (scalar<UINT32_C(0xD800) || scalar>UINT32_C(0xDFFF));
}
#endif
