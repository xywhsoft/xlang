#ifndef XL6_SCALAR_DOMAIN_H
#define XL6_SCALAR_DOMAIN_H
#include "xl6_text_value.h"
#include "xl6_scalar_domain_abi.h"
#include "xl6_value_domain.h"
/* Carrier/range authentication without callbacks, a registry or type-code
 * pointers. Enum u8/u16/u32 use INT; native C unsigned types use UINT. */
static inline bool xl6ScalarDomainAdmitsV1(const xvalue* value,uint32_t domain)
{
    unsigned rule=domain&255u;
    uint32_t family=domain&XL6_SCALAR_DOMAIN_FAMILY_MASK;
    if (!value || !domain || !xrtValueTypeId(value) ||
        !xl6ScalarDomainTagValidV1(domain)) return false;
    xvaluetype type=xrtValueType(value);
    if (family==XL6_SCALAR_DOMAIN_ENUM_V1 && !xl6ScalarEnumDomainV1(rule)) return false;
    if ((rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_I64) ||
        (family!=XL6_SCALAR_DOMAIN_NATIVE_V1 && rule>=XL6_VALUE_ADMIT_U8 && rule<=XL6_VALUE_ADMIT_U32)) {
        int64_t number;
        return type==XVALUE_INT && xrtValueGetInt(value,&number) && xl6IntegerDomainV1(number,rule);
    }
    if (rule>=XL6_VALUE_ADMIT_U8 && rule<=XL6_VALUE_ADMIT_U64) {
        uint64_t number;
        if (type!=XVALUE_UINT || !xrtValueGetUInt(value,&number)) return false;
        return rule==XL6_VALUE_ADMIT_U8 ? number<=UINT8_MAX :
            rule==XL6_VALUE_ADMIT_U16 ? number<=UINT16_MAX :
            rule==XL6_VALUE_ADMIT_U32 ? number<=UINT32_MAX : true;
    }
    if (family!=XL6_SCALAR_DOMAIN_ENUM_V1 && (rule==XL6_VALUE_ADMIT_F32 || rule==XL6_VALUE_ADMIT_F64)) {
        double number;
        return type==XVALUE_FLOAT && xrtValueGetFloat(value,&number) &&
            (rule==XL6_VALUE_ADMIT_F64 || xl6Float32DomainV1(number));
    }
    if(family==XL6_SCALAR_DOMAIN_PRIMITIVE_V1) {
        return rule==XL6_VALUE_ADMIT_BOOL?type==XVALUE_BOOL:rule==XL6_VALUE_ADMIT_CHAR?type==XVALUE_CHAR:
            rule==XL6_VALUE_ADMIT_STRING?type==XVALUE_STRING:rule==XL6_VALUE_ADMIT_BYTES?type==XVALUE_BYTES:
            rule==XL6_VALUE_ADMIT_TIME?type==XVALUE_TIME:false;
    }
    return false;
}
static inline bool xl6ScalarIdentityBindV1(xvalue* value,uint64_t identity,uint32_t domain)
{return domain ? xrtValueScalarDomainBindV1(value,identity,domain) : xrtValueTypeIdBind(value,identity);}
#endif
