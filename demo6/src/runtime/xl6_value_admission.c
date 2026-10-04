#include "src/runtime/xl6_value_admission.h"
#include "src/runtime/xl6_value_admission_internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>

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
    if (rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_I64) {
        int64_t number;
        if (tag!=XVALUE_INT || !xrtValueGetInt(value,&number)) return false;
        switch (rule) {
        case XL6_VALUE_ADMIT_I8: return number>=INT8_MIN && number<=INT8_MAX;
        case XL6_VALUE_ADMIT_I16: return number>=INT16_MIN && number<=INT16_MAX;
        case XL6_VALUE_ADMIT_I32: return number>=INT32_MIN && number<=INT32_MAX;
        default: return true;
        }
    }
    if (rule>=XL6_VALUE_ADMIT_U8 && rule<=XL6_VALUE_ADMIT_U32) {
        int64_t number;
        if (tag!=XVALUE_INT || !xrtValueGetInt(value,&number) || number<0) return false;
        switch (rule) {
        case XL6_VALUE_ADMIT_U8: return number<=UINT8_MAX;
        case XL6_VALUE_ADMIT_U16: return number<=UINT16_MAX;
        case XL6_VALUE_ADMIT_U32: return number<=UINT32_MAX;
        default: return false;
        }
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
        if (isnan(number) || isinf(number)) return true;
        return number>=-FLT_MAX && number<=FLT_MAX && (double)(float)number==number;
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
    if (!output) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!xl6ValueAdmitsV1(source,XL6_VALUE_ADMIT_F32,false,identity)) { xrtSetErrorKind(XERR_TYPE); return false; }
    double value; if (!xrtValueGetFloat(source,&value)) return false;
    *output=(float)value; return true;
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
