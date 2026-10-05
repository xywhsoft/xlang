#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_value_admission.h"
#include "src/runtime/xl6_value_domain.h"
#include "src/runtime/xl6_scalar_type_abi.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_nullable_string.h"
#include <stdlib.h>

typedef struct PrimitivePolicy {
    volatile int32_t refs;
    unsigned int rule;
    bool nullable;
    uint64_t identity;
} PrimitivePolicy;
static bool retain(const void* opaque)
{
    PrimitivePolicy* data=(PrimitivePolicy*)opaque;
    if (!data || xrtRefRetain(&data->refs)<=0) { xrtSetErrorKind(XERR_STATE); return false; }
    return true;
}
static void drop(const void* opaque)
{
    PrimitivePolicy* data=(PrimitivePolicy*)opaque;
    int32_t refs=xrtRefRelease(&data->refs); if (refs<0) abort();
    if (!refs) xrtFree(data);
}
static bool slot(const void* opaque, const void* item)
{
    const PrimitivePolicy* data=opaque;
    if (!item) return false;
    unsigned int rule=data->rule;
    if (rule==XL6_VALUE_ADMIT_STRING) {
        const xstrview* text=item;
        return text->Data ? text->Size<=INT64_MAX : data->nullable && !text->Size;
    }
    if (rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_U32) {
        if (!data->nullable) return xl6IntegerDomainV1(*(const int64_t*)item,rule);
        const XL6_NullableI64V1* value=item;
        return !value->hasValue || xl6IntegerDomainV1(value->value,rule);
    }
    if (rule==XL6_VALUE_ADMIT_CHAR) {
        if (!data->nullable) return xl6CharDomainV1(*(const uint32_t*)item);
        const XL6_NullableCharV1* value=item;
        return !value->hasValue || xl6CharDomainV1(value->value);
    }
    /* The remaining validated C carriers already have their exact primitive
     * domain. Do not read nullable absent payloads or padding. f32 typed slots
     * cannot contain extra f64 precision; erased writes use the predicate. */
    return true;
}
static bool value(const void* opaque, const xvalue* item)
{
    const PrimitivePolicy* data=opaque;
    return xl6ValueAdmitsV1(item,data->rule,data->nullable,data->identity);
}
static bool equivalent(const void* left, const void* right)
{
    const PrimitivePolicy* a=left; const PrimitivePolicy* b=right;
    return a->rule==b->rule && a->nullable==b->nullable && a->identity==b->identity;
}
static bool defaultable(const void* opaque) { (void)opaque; return true; }
static const XL6_SequenceAdmissionOpsV2 ops={retain,drop,slot,value,equivalent,defaultable,NULL,NULL};

static bool physical(const xrttype* type, unsigned int transfer, unsigned int rule, bool nullable)
{
    if (rule==XL6_VALUE_ADMIT_STRING)
        return transfer==2 && (nullable ? xl6NullableStringTypeAbiV1(type) : xl6StringTypeAbiV1(type));
    if (transfer!=1) return false;
    if (nullable) return xl6NullableScalarTypeAbiV1(type,rule);
    if (rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_U32) return type==xrtTypeInt64();
    switch (rule) {
    case XL6_VALUE_ADMIT_BOOL: return type==xrtTypeBool();
    case XL6_VALUE_ADMIT_U64: return type==xrtTypeUInt64();
    case XL6_VALUE_ADMIT_F32: return type==xrtTypeFloat32();
    case XL6_VALUE_ADMIT_F64: return type==xrtTypeFloat64();
    case XL6_VALUE_ADMIT_TIME: return type==xrtTypeTime();
    case XL6_VALUE_ADMIT_CHAR: return xl6CharTypeAbiV1(type);
    default: return false;
    }
}
xvalue* xl6SequencePrimitiveCreateV1(unsigned int role,uint64_t identity, uint64_t element, unsigned int transfer,
    size_t size, const xrttype* type, unsigned int rule, bool nullable, uint64_t valueIdentity)
{
    if (!identity || !element || !valueIdentity || !type || type->Size!=size ||
        !physical(type,transfer,rule,nullable)) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    PrimitivePolicy* data=xrtMalloc(sizeof(*data));
    if (!data) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return NULL; }
    *data=(PrimitivePolicy){.refs=1,.rule=rule,.nullable=nullable,.identity=valueIdentity};
    XL6_SequenceAdmissionV3 admission={&ops,data,element,transfer,size,type,valueIdentity};
    return xl6SequenceCreateAdmittedV1((XL6_SequenceRoleV1)role,identity,&admission);
}
