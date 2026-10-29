#include "src/runtime/xl6_array_extract_view.h"
#include "src/runtime/xl6_scalar_type_abi.h"
#include "src/runtime/xl6_c_scalar_domain.h"
#include "src/runtime/xl6_value_admission.h"

/* Actual scalar/optional union members provide both alignment and C effective
 * types. A generic aligned byte buffer would not authorize scalar lvalues. */
typedef union TrivialScratch {
    bool boolean; int8_t i8; uint8_t u8; int16_t i16; uint16_t u16;
    int32_t i32; uint32_t u32; int64_t i64; uint64_t u64; float f32; double f64;
    XL6_NullableBoolV1 nb; XL6_NullableI64V1 ni; XL6_NullableU64V1 nu;
    XL6_NullableF32V1 nf; XL6_NullableF64V1 nd; XL6_NullableTimeV1 nt;
    XL6_NullableCharV1 nc;
    XL6_NullableNativeI8V1 ni8; XL6_NullableNativeU8V1 nu8;
    XL6_NullableNativeI16V1 ni16; XL6_NullableNativeU16V1 nu16;
    XL6_NullableNativeI32V1 ni32; XL6_NullableNativeU32V1 nu32;
} TrivialScratch;

static bool logicalType(const xrttype* base,unsigned int rule)
{
    if (rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_U32) return base==xrtTypeInt64();
    switch (rule) {
    case XL6_VALUE_ADMIT_BOOL: return base==xrtTypeBool();
    case XL6_VALUE_ADMIT_U64: return base==xrtTypeUInt64();
    case XL6_VALUE_ADMIT_F32: return base==xrtTypeFloat32();
    case XL6_VALUE_ADMIT_F64: return base==xrtTypeFloat64();
    case XL6_VALUE_ADMIT_TIME: return base==xrtTypeTime();
    case XL6_VALUE_ADMIT_CHAR: return xl6CharTypeAbiV1(base);
    default: return false;
    }
}

bool xl6ArrayExtractTrivialV1(const xvalue* source,uint64_t identity,uint64_t element,
    int64_t index,bool pop,const xrttype* type,const xrttype* base,
    const xvalueconverter* converter,unsigned int rule,uint64_t baseIdentity,void* output)
{
    XL6_ArrayExtractView view={0}; TrivialScratch result={0}; bool ok=false;
    if (!xl6ArrayExtractBegin(source,identity,element,index,pop,&view)) goto done;
    bool nullable=type!=base;
    bool physical=rule==XL6_VALUE_ADMIT_NONE ? xl6CScalarTypeV1(base) &&
        (!nullable || xl6NullableCScalarTypeAbiV1(type,base)) :
        logicalType(base,rule) && (!nullable || xl6NullableScalarTypeAbiV1(type,rule));
    if (!output || !baseIdentity || !physical || !type || type->Size>sizeof(result)) {
        xrtSetErrorKind(XERR_ARGUMENT); goto done;
    }
    if (view.owner) {
        ok=xl6ArrayExtractSlotMatches(&view,1,type->Size,type) &&
            xrtTypedArrayTake(xl6SequenceStorageV1(view.owner),view.at,&result);
    } else {
        bool admits=rule!=XL6_VALUE_ADMIT_NONE ?
            xl6ValueAdmitsV1(view.item,rule,nullable,baseIdentity) :
            (nullable && xrtValueType(view.item)==XVALUE_NULL) ||
                xl6CScalarValueV1(base,baseIdentity,view.item);
        if (!admits) xrtSetErrorKind(XERR_TYPE);
        else ok=xrtValueToTyped(view.item,type,&result,converter);
        if (ok) ok=xl6ArrayExtractWireCommit(&view);
    }
    if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
done:
    if (!xl6ArrayExtractFinish(&view,ok)) return false;
    memcpy(output,&result,type->Size); return true;
}
