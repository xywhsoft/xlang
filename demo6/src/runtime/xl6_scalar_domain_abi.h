#ifndef XL6_SCALAR_DOMAIN_ABI_H
#define XL6_SCALAR_DOMAIN_ABI_H
#include "xl6_value_admission_abi.h"
#include <stdint.h>
#include <stdbool.h>
/* Caller-owned XRT metadata namespaces, versioned and data-only. A native C
 * scalar is not a numeric enum even when identity, width and payload match.
 * The low byte is a closed ValueAdmission rule, not an inferred value width. */
#define XL6_SCALAR_DOMAIN_ENUM_V1 UINT32_C(0x584C0100)
#define XL6_SCALAR_DOMAIN_NATIVE_V1 UINT32_C(0x584C0200)
#define XL6_SCALAR_DOMAIN_PRIMITIVE_V1 UINT32_C(0x584C0300)
#define XL6_SCALAR_DOMAIN_FAMILY_MASK UINT32_C(0xFFFFFF00)
static inline uint32_t xl6ScalarEnumDomainV1(unsigned rule)
{
    return rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_U64
        ? XL6_SCALAR_DOMAIN_ENUM_V1 | rule : 0;
}
static inline uint32_t xl6ScalarPrimitiveDomainV1(unsigned rule)
{
    return rule>=XL6_VALUE_ADMIT_BOOL && rule<=XL6_VALUE_ADMIT_TIME
        ? XL6_SCALAR_DOMAIN_PRIMITIVE_V1|rule : 0;
}
static inline bool xl6ScalarDomainTagValidV1(uint32_t domain)
{
    unsigned rule=domain&255u;uint32_t family=domain&XL6_SCALAR_DOMAIN_FAMILY_MASK;
    return family==XL6_SCALAR_DOMAIN_ENUM_V1 ? xl6ScalarEnumDomainV1(rule)!=0 :
        family==XL6_SCALAR_DOMAIN_PRIMITIVE_V1 ? xl6ScalarPrimitiveDomainV1(rule)!=0 :
        family==XL6_SCALAR_DOMAIN_NATIVE_V1 && rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_F64;
}
#endif
