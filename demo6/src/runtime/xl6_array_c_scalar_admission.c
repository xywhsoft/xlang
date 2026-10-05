#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_value_domain.h"
#include <stdlib.h>
#include <string.h>

/* C ABI scalars use their actual native-width XRT descriptor, not the
 * language's widened i64/u8 boxed convention. Only resident builtin singleton
 * descriptors are accepted; foreign callbacks/lifetimes are not inferred. */
typedef struct CScalarPolicy {
    volatile int32_t refs;
    const xrttype* type;
    uint64_t identity;
} CScalarPolicy;
static bool physical(const xrttype* type)
{
    return type && (type==xrtTypeBool() || type==xrtTypeInt8() || type==xrtTypeUInt8() ||
        type==xrtTypeInt16() || type==xrtTypeUInt16() || type==xrtTypeInt32() ||
        type==xrtTypeUInt32() || type==xrtTypeInt64() || type==xrtTypeUInt64() ||
        type==xrtTypeFloat32() || type==xrtTypeFloat64() || type==xrtTypeType());
}
static bool retain(const void* opaque)
{
    CScalarPolicy* data=(CScalarPolicy*)opaque;
    if (!data || xrtRefRetain(&data->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void drop(const void* opaque)
{
    CScalarPolicy* data=(CScalarPolicy*)opaque;
    int32_t refs=xrtRefRelease(&data->refs); if (refs<0) abort();
    if (!refs) xrtFree(data);
}
static bool slot(const void* opaque,const void* item)
{
    const CScalarPolicy* data=opaque;
    if (!item) return false;
    if (data->type==xrtTypeBool()) {
        /* Do not read a possibly malformed native bool through a bool lvalue. */
        unsigned char representation=0;
        _Static_assert(sizeof(bool)==1,"Supported native bool has a one-byte representation");
        memcpy(&representation,item,1); return representation<=1;
    }
    return true;
}
static bool signedDomain(int64_t number,size_t size)
{
    switch (size) {
    case 1: return number>=INT8_MIN && number<=INT8_MAX;
    case 2: return number>=INT16_MIN && number<=INT16_MAX;
    case 4: return number>=INT32_MIN && number<=INT32_MAX;
    case 8: return true;
    default: return false;
    }
}
static bool unsignedDomain(uint64_t number,size_t size)
{
    switch (size) {
    case 1: return number<=UINT8_MAX;
    case 2: return number<=UINT16_MAX;
    case 4: return number<=UINT32_MAX;
    case 8: return true;
    default: return false;
    }
}
static bool value(const void* opaque,const xvalue* item)
{
    const CScalarPolicy* data=opaque;
    if (!item) return false;
    uint64_t actual=xrtValueTypeId(item);
    if (actual && actual!=data->identity) return false;
    xvaluetype tag=xrtValueType(item);
    switch (data->type->Kind) {
    case XRT_TYPE_BOOL: return tag==XVALUE_BOOL;
    case XRT_TYPE_SIGNED_INT: {
        int64_t number;
        return tag==XVALUE_INT && xrtValueGetInt(item,&number) && signedDomain(number,data->type->Size);
    }
    case XRT_TYPE_TYPE:
        /* A uint64 payload alone is not a language type value. */
        if (!actual) return false;
        /* fall through */
    case XRT_TYPE_UNSIGNED_INT: {
        uint64_t number;
        return tag==XVALUE_UINT && xrtValueGetUInt(item,&number) && unsignedDomain(number,data->type->Size);
    }
    case XRT_TYPE_FLOAT: {
        double number;
        return tag==XVALUE_FLOAT && xrtValueGetFloat(item,&number) &&
            (data->type==xrtTypeFloat64() || xl6Float32DomainV1(number));
    }
    default: return false;
    }
}
static bool equivalent(const void* a,const void* b)
{
    const CScalarPolicy* left=a; const CScalarPolicy* right=b;
    return left->type==right->type && left->identity==right->identity;
}
static bool defaultable(const void* opaque) { (void)opaque; return true; }
static const XL6_SequenceAdmissionOpsV1 ops={retain,drop,slot,value,equivalent,defaultable,NULL};

xvalue* xl6ArrayCScalarCreateV1(uint64_t identity,uint64_t element,const xrttype* type,uint64_t boxedIdentity)
{
    if (!identity || !element || element!=boxedIdentity || !physical(type)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    CScalarPolicy* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(CScalarPolicy){.refs=1,.type=type,.identity=boxedIdentity};
    XL6_SequenceAdmissionV2 admission={&ops,data,element,1,type->Size,type,boxedIdentity};
    xvalue* result=xl6ArrayCreateV1(identity,element,1,type->Size,type);
    if (result) {
        XL6_Sequence* owner=xl6SequenceBeginRoleV1(result,XL6_SEQUENCE_ARRAY_V1,identity,element,1,type->Size,type);
        if (!owner || !xl6SequenceAdmissionTakeV2(owner,&admission)) {
            if (owner) xl6SequenceEndV1(owner);
            xerror* error=xrtTakeError(); xrtValueRelease(result); result=NULL;
            xrtClearError(); xrtSetErrorTake(error);
        } else xl6SequenceEndV1(owner);
    }
    xl6SequenceAdmissionDropV2(&admission); return result;
}
