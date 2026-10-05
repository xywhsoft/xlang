#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_value_domain.h"
#include "src/runtime/xl6_scalar_type_abi.h"
#include <stdlib.h>

typedef struct NumericEnumPolicy {
    volatile int32_t refs;
    unsigned int rule;
    bool nullable;
    uint64_t identity;
} NumericEnumPolicy;
static bool retain(const void* opaque)
{
    NumericEnumPolicy* data=(NumericEnumPolicy*)opaque;
    if (!data || xrtRefRetain(&data->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void drop(const void* opaque)
{
    NumericEnumPolicy* data=(NumericEnumPolicy*)opaque;
    int32_t refs=xrtRefRelease(&data->refs); if (refs<0) abort();
    if (!refs) xrtFree(data);
}
static bool slot(const void* opaque,const void* item)
{
    const NumericEnumPolicy* data=opaque;
    if (!item) return false;
    const unsigned char* payload=item;
    if (data->nullable) {
        unsigned char present; memcpy(&present,item,1);
        if (present>1) return false; /* Never evaluate malformed bool representation. */
        if (!present) return true; /* Absent payload and padding are not values. */
        payload+=data->rule==XL6_VALUE_ADMIT_U64 ? offsetof(XL6_NullableU64V1,value) :
            offsetof(XL6_NullableI64V1,value);
    }
    if (data->rule==XL6_VALUE_ADMIT_U64) return true;
    int64_t number; memcpy(&number,payload,sizeof(number));
    return xl6IntegerDomainV1(number,data->rule);
}
static bool value(const void* opaque,const xvalue* item)
{
    const NumericEnumPolicy* data=opaque;
    if (!item) return false;
    xvaluetype tag=xrtValueType(item);
    if (tag==XVALUE_NULL) return data->nullable;
    /* The full backing integer domain is legal, not only declared variants.
     * Unlike primitive admission, an unbound integer is never an enum. */
    if (xrtValueTypeId(item)!=data->identity) return false;
    if (data->rule==XL6_VALUE_ADMIT_U64) return tag==XVALUE_UINT;
    int64_t number;
    return tag==XVALUE_INT && xrtValueGetInt(item,&number) && xl6IntegerDomainV1(number,data->rule);
}
static bool equivalent(const void* left,const void* right)
{
    const NumericEnumPolicy* a=left; const NumericEnumPolicy* b=right;
    return a->rule==b->rule && a->nullable==b->nullable && a->identity==b->identity;
}
static bool defaultable(const void* opaque) { (void)opaque; return true; }
static const XL6_SequenceAdmissionOpsV2 ops={retain,drop,slot,value,equivalent,defaultable,NULL,NULL};
xvalue* xl6SequenceNumericEnumCreateV1(unsigned int role,uint64_t identity,uint64_t element,const xrttype* type,
    unsigned int rule,bool nullable,uint64_t boxedIdentity)
{
    bool integer=rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_U64;
    if (!identity || !element || !boxedIdentity || !integer || !type ||
        (nullable ? element==boxedIdentity : element!=boxedIdentity) ||
        (nullable ? !xl6NullableScalarTypeAbiV1(type,rule) :
            type!=(rule==XL6_VALUE_ADMIT_U64 ? xrtTypeUInt64() : xrtTypeInt64()))) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    NumericEnumPolicy* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(NumericEnumPolicy){.refs=1,.rule=rule,.nullable=nullable,.identity=boxedIdentity};
    XL6_SequenceAdmissionV3 admission={&ops,data,element,1,type->Size,type,boxedIdentity};
    return xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,identity,&admission);
}
