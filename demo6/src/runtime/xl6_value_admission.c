#include "src/runtime/xl6_value_admission.h"
#include "src/runtime/xl6_value_admission_internal.h"
#include "src/runtime/xl6_value_domain.h"
#include <string.h>

xvalue* xl6ValueTryBytesV1(const xvalue* value, uint64_t identity)
{
    return xl6ValueAdmitsV1(value, XL6_VALUE_ADMIT_BYTES, false, identity)
        ? xrtValueRetain(value) : xrtValueNull();
}

bool xl6ValueTryReadIntegerV1(const xvalue* source, unsigned int rule, uint64_t identity, void* output)
{
    if (!output || !identity || rule < XL6_VALUE_ADMIT_I8 || rule > XL6_VALUE_ADMIT_U64 ||
        !xl6ValueAdmitsV1(source, rule, false, identity)) return false;
    if (rule == XL6_VALUE_ADMIT_U64) {
        uint64_t value;
        if (!xrtValueGetUInt(source, &value)) return false;
        memcpy(output, &value, sizeof(value));
    } else {
        int64_t value;
        if (!xrtValueGetInt(source, &value)) return false;
        memcpy(output, &value, sizeof(value));
    }
    return true;
}

bool xl6ValueAdmitsV1(const xvalue* value, unsigned int rule, bool nullable, uint64_t identity)
{
    if (!value || rule <= XL6_VALUE_ADMIT_NONE || rule > XL6_VALUE_ADMIT_TIME) return false;
    xvaluetype tag=xrtValueType(value);
    if (rule==XL6_VALUE_ADMIT_ANY) return tag>=XVALUE_NULL && tag<=XVALUE_CHAR;
    if (tag==XVALUE_NULL) return nullable;
    /* A nominal inline class/union can share a primitive physical tag (Xid
     * uses bytes). Its explicit identity is not permission to reinterpret it
     * as that primitive. Unbound XRT primitives remain valid inputs. */
    uint64_t actual=xrtValueTypeId(value);
    if (actual && actual!=identity) return false;
    if (rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_U32) {
        int64_t number;
        if (tag!=XVALUE_INT || !xrtValueGetInt(value,&number)) return false;
        return xl6IntegerDomainV1(number,rule);
    }
    switch (rule) {
    case XL6_VALUE_ADMIT_U64: return tag==XVALUE_UINT;
    case XL6_VALUE_ADMIT_BOOL: return tag==XVALUE_BOOL;
    case XL6_VALUE_ADMIT_F64: return tag==XVALUE_FLOAT;
    case XL6_VALUE_ADMIT_F32: {
        double number;
        if (tag!=XVALUE_FLOAT || !xrtValueGetFloat(value,&number)) return false;
        /* The retained source must already represent an f32, not a value
         * silently rounded on each typed read. Avoid out-of-range C casts. */
        return xl6Float32DomainV1(number);
    }
    case XL6_VALUE_ADMIT_CHAR: return tag==XVALUE_CHAR;
    case XL6_VALUE_ADMIT_STRING: return tag==XVALUE_STRING;
    case XL6_VALUE_ADMIT_BYTES: return tag==XVALUE_BYTES;
    case XL6_VALUE_ADMIT_TIME: return tag==XVALUE_TIME;
    default: return false;
    }
}

xvalue* xl6ValueCheckedRetainV1(const xvalue* source, unsigned int rule, bool nullable, uint64_t identity)
{
    if (!xl6ValueAdmitsV1(source,rule,nullable,identity)) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return xrtValueRetain(source);
}
bool xl6ValueReadF32V1(const xvalue* source, uint64_t identity, float* output)
{
    if (!output || !identity) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!source || xrtValueType(source)!=XVALUE_FLOAT ||
        (xrtValueTypeId(source) && xrtValueTypeId(source)!=identity)) { xrtSetErrorKind(XERR_TYPE); return false; }
    double value; if (!xrtValueGetFloat(source,&value)) return false;
    if(!xl6Float32DomainV1(value)) { xrtSetErrorKind(XERR_TYPE); return false; }
    *output=(float)value; return true;
}
bool xl6ValueTryReadF32V1(const xvalue* source,uint64_t identity,bool* present,float* output)
{
    if(!source || !identity || !present || !output) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if(xrtValueType(source)!=XVALUE_FLOAT || (xrtValueTypeId(source) && xrtValueTypeId(source)!=identity)) {
        *present=false; return true;
    }
    double value; if(!xrtValueGetFloat(source,&value))return false;
    if(!xl6Float32DomainV1(value)) { *present=false; return true; }
    *output=(float)value; *present=true; return true;
}
bool xl6ValueIntegerSlotAdmitsV1(const void* slot, unsigned int rule)
{
    if (!slot || rule < XL6_VALUE_ADMIT_I8 || rule > XL6_VALUE_ADMIT_U64) return false;
    if (rule == XL6_VALUE_ADMIT_U64) return true;
    int64_t value; memcpy(&value,slot,sizeof(value));
    return xl6IntegerDomainV1(value,rule);
}
bool xl6ValueReadIntegerV1(const xvalue* source, unsigned int rule,
    uint64_t identity, bool nominal, void* output)
{
    if (!output || rule < XL6_VALUE_ADMIT_I8 || rule > XL6_VALUE_ADMIT_U64 || !identity) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    if (!xl6ValueAdmitsV1(source,rule,false,identity) ||
        (nominal && xrtValueTypeId(source) != identity)) {
        xrtSetErrorKind(XERR_TYPE); return false;
    }
    if (rule == XL6_VALUE_ADMIT_U64) {
        uint64_t value; if (!xrtValueGetUInt(source,&value)) return false;
        memcpy(output,&value,sizeof(value));
    } else {
        int64_t value; if (!xrtValueGetInt(source,&value)) return false;
        memcpy(output,&value,sizeof(value));
    }
    return true;
}
xvalue* xl6ValueBoxIntegerV1(const void* slot, unsigned int rule, uint64_t identity, bool nominal)
{
    if (!slot || rule < XL6_VALUE_ADMIT_I8 || rule > XL6_VALUE_ADMIT_U64 || !identity) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    if (!xl6ValueIntegerSlotAdmitsV1(slot,rule)) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    xvalue* value;
    if (rule == XL6_VALUE_ADMIT_U64) {
        uint64_t number; memcpy(&number,slot,sizeof(number)); value=xrtValueUInt(number);
    } else {
        int64_t number; memcpy(&number,slot,sizeof(number)); value=xrtValueInt(number);
    }
    if (!value || !nominal || xrtValueTypeIdBind(value,identity)) return value;
    /* Binding can allocate. Preserve its error across destructive rollback. */
    xerror* error=xrtTakeError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(error);
    return NULL;
}
typedef struct PrimitiveContext { unsigned int rule; bool nullable; uint64_t identity; } PrimitiveContext;
static bool admits(const xvalue* value, const void* opaque)
{
    const PrimitiveContext* context=opaque;
    return xl6ValueAdmitsV1(value,context->rule,context->nullable,context->identity);
}
xvalue* xl6CollectionCheckedTakeV1(xvalue* source, int container, unsigned int rule, bool nullable, uint64_t identity)
{
    if (rule>XL6_VALUE_ADMIT_TIME || (rule==XL6_VALUE_ADMIT_NONE && nullable))
        return xl6AdmissionFailure(source,NULL,XERR_ARGUMENT);
    const PrimitiveContext context={rule,nullable,identity};
    return xl6AdmissionCollectionTake(source,container,
        rule==XL6_VALUE_ADMIT_NONE || rule==XL6_VALUE_ADMIT_ANY ? NULL : admits,&context);
}
