#ifndef XL6_C_SCALAR_DOMAIN_H
#define XL6_C_SCALAR_DOMAIN_H
#include "xl6_value_domain.h"
#include "xl6_scalar_domain_abi.h"
#include <string.h>
/* Closed builtin native scalars; never dereference an unrecognized pointer. */
static inline uint32_t xl6CScalarDomainTagV1(const xrttype* type)
{
    if(!type)return 0;
    unsigned rule=0;
    if(type->Kind==XRT_TYPE_FLOAT) rule=type->Size==4?XL6_VALUE_ADMIT_F32:type->Size==8?XL6_VALUE_ADMIT_F64:0;
    else if(type->Kind==XRT_TYPE_SIGNED_INT || type->Kind==XRT_TYPE_UNSIGNED_INT) {
        static const unsigned signedRules[]={XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I16,XL6_VALUE_ADMIT_I32,XL6_VALUE_ADMIT_I64};
        static const unsigned unsignedRules[]={XL6_VALUE_ADMIT_U8,XL6_VALUE_ADMIT_U16,XL6_VALUE_ADMIT_U32,XL6_VALUE_ADMIT_U64};
        unsigned at=type->Size==1?0:type->Size==2?1:type->Size==4?2:type->Size==8?3:4;
        if(at<4)rule=type->Kind==XRT_TYPE_SIGNED_INT?signedRules[at]:unsignedRules[at];
    }
    return rule?XL6_SCALAR_DOMAIN_NATIVE_V1|rule:0;
}
static inline bool xl6CScalarTypeV1(const xrttype* type)
{
    return type && (type==xrtTypeBool() || type==xrtTypeInt8() || type==xrtTypeUInt8() ||
        type==xrtTypeInt16() || type==xrtTypeUInt16() || type==xrtTypeInt32() ||
        type==xrtTypeUInt32() || type==xrtTypeInt64() || type==xrtTypeUInt64() ||
        type==xrtTypeFloat32() || type==xrtTypeFloat64() || type==xrtTypeType() ||
        type==xrtTypePointer());
}
static inline bool xl6CScalarSlotV1(const xrttype* type,const void* item)
{
    if (!item) return false;
    if (type==xrtTypeBool()) {
        unsigned char representation=0;
        _Static_assert(sizeof(bool)==1,"Supported bool requires one-byte representation");
        memcpy(&representation,item,1); return representation<=1;
    }
    return true;
}
static inline bool xl6CScalarSignedDomainV1(int64_t number,size_t size)
{
    switch (size) {
    case 1: return number>=INT8_MIN && number<=INT8_MAX;
    case 2: return number>=INT16_MIN && number<=INT16_MAX;
    case 4: return number>=INT32_MIN && number<=INT32_MAX;
    case 8: return true;
    default: return false;
    }
}
static inline bool xl6CScalarUnsignedDomainV1(uint64_t number,size_t size)
{
    switch (size) {
    case 1: return number<=UINT8_MAX;
    case 2: return number<=UINT16_MAX;
    case 4: return number<=UINT32_MAX;
    case 8: return true;
    default: return false;
    }
}
static inline bool xl6CScalarValueV1(const xrttype* type,uint64_t identity,const xvalue* item)
{
    if (!item) return false;
    uint64_t actual=xrtValueTypeId(item);
    if (actual && actual!=identity) return false;
    if(actual && xrtValueScalarDomainV1(item)!=xl6CScalarDomainTagV1(type)) return false;
    xvaluetype tag=xrtValueType(item);
    switch (type->Kind) {
    /* Raw C interop may supply an unbound pointer. A typed collection
     * projection carries its exact point<T> identity; unrelated bound
     * pointers were rejected above. Never inspect or own the pointee. */
    case XRT_TYPE_POINTER: return tag==XVALUE_POINTER;
    case XRT_TYPE_BOOL: return tag==XVALUE_BOOL;
    case XRT_TYPE_SIGNED_INT: {
        int64_t number;
        return tag==XVALUE_INT && xrtValueGetInt(item,&number) && xl6CScalarSignedDomainV1(number,type->Size);
    }
    case XRT_TYPE_TYPE:
        if (!actual) return false; /* Raw UInt is not a type value. */
        /* fall through */
    case XRT_TYPE_UNSIGNED_INT: {
        uint64_t number;
        return tag==XVALUE_UINT && xrtValueGetUInt(item,&number) && xl6CScalarUnsignedDomainV1(number,type->Size);
    }
    case XRT_TYPE_FLOAT: {
        double number;
        return tag==XVALUE_FLOAT && xrtValueGetFloat(item,&number) &&
            (type==xrtTypeFloat64() || xl6Float32DomainV1(number));
    }
    default: return false;
    }
}
#endif
