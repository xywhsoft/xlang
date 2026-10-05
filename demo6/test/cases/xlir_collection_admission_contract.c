#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <wchar.h>
#include <stdlib.h>

static void rejected(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output output; XL6_DiagList diagnostics; size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics); assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,ir,"REJECTED_COLLECTION_ABI",&diagnostics)
                        : xl6IrCgenEmitC(&output,ir,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);
        assert(size==0); xrtFree(text); xl6OutputDiscard(&output); xl6DiagFree(&diagnostics);
    }
}

/* Independent physical oracle: instantiate each emitted recipe with the real
 * resident descriptor/factory. These native probes are NOT a claim that the
 * generated language Array carrier has already changed. */
static const xrttype* descriptor(XL6_IrNativeSlotAbi abi)
{
    switch (abi) {
    case XL6_IR_NATIVE_SLOT_BOOL: return xrtTypeBool();
    case XL6_IR_NATIVE_SLOT_I64: return xrtTypeInt64();
    case XL6_IR_NATIVE_SLOT_U64: return xrtTypeUInt64();
    case XL6_IR_NATIVE_SLOT_F32: return xrtTypeFloat32();
    case XL6_IR_NATIVE_SLOT_F64: return xrtTypeFloat64();
    case XL6_IR_NATIVE_SLOT_TIME: return xrtTypeTime();
    case XL6_IR_NATIVE_SLOT_TYPE_ID: return xrtTypeType();
    case XL6_IR_NATIVE_SLOT_C_CHAR: return CHAR_MIN<0 ? xrtTypeInt8() : xrtTypeUInt8();
    case XL6_IR_NATIVE_SLOT_C_SCHAR: return xrtTypeInt8();
    case XL6_IR_NATIVE_SLOT_C_UCHAR: return xrtTypeUInt8();
    case XL6_IR_NATIVE_SLOT_C_SHORT: return xrtTypeInt16();
    case XL6_IR_NATIVE_SLOT_C_USHORT: return xrtTypeUInt16();
    case XL6_IR_NATIVE_SLOT_C_INT: return xrtTypeInt32();
    case XL6_IR_NATIVE_SLOT_C_UINT: return xrtTypeUInt32();
    case XL6_IR_NATIVE_SLOT_C_LONG: return sizeof(long)==4 ? xrtTypeInt32() : xrtTypeInt64();
    case XL6_IR_NATIVE_SLOT_C_ULONG: return sizeof(unsigned long)==4 ? xrtTypeUInt32() : xrtTypeUInt64();
    case XL6_IR_NATIVE_SLOT_C_LONGLONG: return xrtTypeInt64();
    case XL6_IR_NATIVE_SLOT_C_ULONGLONG: return xrtTypeUInt64();
    case XL6_IR_NATIVE_SLOT_C_SIZE: case XL6_IR_NATIVE_SLOT_C_UINTPTR:
        return sizeof(size_t)==4 ? xrtTypeUInt32() : xrtTypeUInt64();
    case XL6_IR_NATIVE_SLOT_C_SSIZE: case XL6_IR_NATIVE_SLOT_C_PTRDIFF: case XL6_IR_NATIVE_SLOT_C_INTPTR:
        return sizeof(ptrdiff_t)==4 ? xrtTypeInt32() : xrtTypeInt64();
    case XL6_IR_NATIVE_SLOT_C_BOOL: return xrtTypeBool();
    case XL6_IR_NATIVE_SLOT_C_FLOAT: return xrtTypeFloat32();
    case XL6_IR_NATIVE_SLOT_C_DOUBLE: return xrtTypeFloat64();
    case XL6_IR_NATIVE_SLOT_C_WCHAR:
        return sizeof(wchar_t)==2 ? (WCHAR_MIN<0 ? xrtTypeInt16() : xrtTypeUInt16()) :
            (WCHAR_MIN<0 ? xrtTypeInt32() : xrtTypeUInt32());
    case XL6_IR_NATIVE_SLOT_CHAR: return xl6CharTypeV1();
    case XL6_IR_NATIVE_SLOT_STRING: return xl6StringTypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_STRING: return xl6NullableStringTypeV1();
    case XL6_IR_NATIVE_SLOT_XVALUE_CARRIER: return xrtTypeValue();
    case XL6_IR_NATIVE_SLOT_FUTURE_PTR: return xrtTypeFuture();
    case XL6_IR_NATIVE_SLOT_NULLABLE_BOOL: return xl6NullableBoolTypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_I64: return xl6NullableI64TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_U64: return xl6NullableU64TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_TYPE_ID: return xl6NullableU64TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I8: return xl6NullableNativeI8TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U8: return xl6NullableNativeU8TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I16: return xl6NullableNativeI16TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U16: return xl6NullableNativeU16TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_I32: return xl6NullableNativeI32TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_NATIVE_U32: return xl6NullableNativeU32TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_F32: return xl6NullableF32TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_F64: return xl6NullableF64TypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_TIME: return xl6NullableTimeTypeV1();
    case XL6_IR_NATIVE_SLOT_NULLABLE_CHAR: return xl6NullableCharTypeV1();
    default: return NULL;
    }
}
static unsigned residentRecipe(const XL6_IrModule* ir, const XL6_IrType* type)
{
    const XL6_IrArrayLayout* plan=&type->arrayLayout;
    if (plan->abi != XL6_IR_ARRAY_LAYOUT_PRIMITIVE_V2 &&
        plan->abi != XL6_IR_ARRAY_LAYOUT_REFERENCE_V2 && plan->abi != XL6_IR_ARRAY_LAYOUT_NOMINAL_V2 &&
        plan->abi != XL6_IR_ARRAY_LAYOUT_C_SCALAR_V1 &&
        plan->abi != XL6_IR_ARRAY_LAYOUT_NULLABLE_C_SCALAR_V1 &&
        plan->abi != XL6_IR_ARRAY_LAYOUT_NUMERIC_ENUM_V1 &&
        plan->abi != XL6_IR_ARRAY_LAYOUT_CALLABLE_REFERENCE_V1 &&
        plan->abi != XL6_IR_ARRAY_LAYOUT_FUTURE_REFERENCE_V1 &&
        plan->abi != XL6_IR_ARRAY_LAYOUT_SEQUENCE_REFERENCE_V1) return 0;
    const XL6_IrType* element=xl6IrTypeGet(&ir->types,plan->elementType);
    const XL6_IrType* base=xl6IrTypeGet(&ir->types,plan->baseElementType);
    assert(element && base && type->typeValueId && element->typeValueId && base->typeValueId);
    const xrttype* physical=descriptor(plan->slotAbi);
    assert(physical);
    xvalue* value=NULL;
    if (plan->abi == XL6_IR_ARRAY_LAYOUT_FUTURE_REFERENCE_V1) {
        assert(base->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR && !element->nativeSlotAbi &&
            plan->slotAbi==XL6_IR_NATIVE_SLOT_FUTURE_PTR && plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF);
        value=xl6ArrayFutureCreateV1(type->typeValueId,element->typeValueId,base->typeValueId,plan->nullable);
    } else if (plan->abi == XL6_IR_ARRAY_LAYOUT_CALLABLE_REFERENCE_V1) {
        assert(base->kind==XL6_IR_TYPE_FUNCTION && base->cAbi==XL6_IR_C_ABI_CLOSURE_PTR &&
            xl6IrRuntimeCallableClosureToXValueSignatureSupported(ir,plan->baseElementType) &&
            !element->nativeSlotAbi && !plan->admissionAbi &&
            plan->codecAbi==XL6_IR_ARRAY_CODEC_CLOSURE_VALUE_V1 &&
            plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN);
        value=xl6ArrayCallableCreateV1(type->typeValueId,element->typeValueId,base->typeValueId,plan->nullable);
    } else if (plan->abi == XL6_IR_ARRAY_LAYOUT_SEQUENCE_REFERENCE_V1) {
        assert(base->collectionAbi==XL6_IR_COLLECTION_ARRAY &&
            plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN && !plan->admissionAbi);
        value=xl6ArraySequenceCreateV1(type->typeValueId,element->typeValueId,base->typeValueId,plan->nullable);
    } else if (plan->abi == XL6_IR_ARRAY_LAYOUT_NUMERIC_ENUM_V1) {
        assert(base->kind==XL6_IR_TYPE_NUMERIC_ENUM && !element->xvalueAdmissionAbi &&
            plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY &&
            plan->admissionAbi>=XL6_VALUE_ADMIT_I8 && plan->admissionAbi<=XL6_VALUE_ADMIT_U64);
        value=xl6ArrayNumericEnumCreateV1(type->typeValueId,element->typeValueId,
            physical,plan->admissionAbi,plan->nullable,base->typeValueId);
    } else if (plan->abi == XL6_IR_ARRAY_LAYOUT_NULLABLE_C_SCALAR_V1) {
        const xrttype* basePhysical=descriptor(base->nativeSlotAbi);
        assert(basePhysical && plan->nullable && !plan->admissionAbi &&
            plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY &&
            plan->defaultAbi==XL6_IR_ARRAY_DEFAULT_ABSENT);
        value=xl6ArrayNullableCScalarCreateV1(type->typeValueId,element->typeValueId,
            physical,basePhysical,base->typeValueId);
    } else if (plan->abi == XL6_IR_ARRAY_LAYOUT_C_SCALAR_V1) {
        assert(!plan->nullable && !plan->admissionAbi &&
            plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY &&
            plan->defaultAbi==XL6_IR_ARRAY_DEFAULT_ZERO);
        value=xl6ArrayCScalarCreateV1(type->typeValueId,element->typeValueId,physical,base->typeValueId);
    } else if (plan->abi == XL6_IR_ARRAY_LAYOUT_PRIMITIVE_V2) {
        value=xl6ArrayPrimitiveCreateV2(type->typeValueId,element->typeValueId,
            plan->transferAbi,physical->Size,physical,plan->admissionAbi,plan->nullable,base->typeValueId);
    } else if (plan->abi == XL6_IR_ARRAY_LAYOUT_REFERENCE_V2) {
        assert(plan->slotAbi == XL6_IR_NATIVE_SLOT_XVALUE_CARRIER &&
            plan->transferAbi == XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN);
        value=xl6ArrayReferenceCreateV2(type->typeValueId,element->typeValueId,plan->admissionAbi,
            plan->nullable,plan->admissionAbi == XL6_VALUE_ADMIT_ANY ? 0 : base->typeValueId);
    } else {
        const XL6_IrNominalIdentitySet* set=xl6IrNominalIdentitySetGet(ir,plan->baseElementType,NULL);
        assert(set && set->identityCount <= ir->nominalIdentityCount &&
            set->firstIdentity <= ir->nominalIdentityCount-set->identityCount);
        XL6_NominalAdmissionV1 schema={.kind=(XL6_NominalAdmissionKindV1)set->kind,
            .nonObjectIdentity=set->nonObjectTypeId,
            .identities=set->identityCount ? &ir->nominalIdentities[set->firstIdentity] : NULL,
            .count=set->identityCount};
        value=xl6ArrayNominalCreateV2(type->typeValueId,element->typeValueId,&schema,plan->nullable,base->typeValueId);
    }
    assert(value && !xrtGetError() && xrtValueType(value) == XVALUE_HANDLE);
#define LAYOUT type->typeValueId,element->typeValueId,plan->transferAbi,physical->Size,physical
    assert(xl6ArraySizeV1(value,LAYOUT,false) == 0 && !xrtGetError());
    bool defaultable=plan->defaultAbi != XL6_IR_ARRAY_DEFAULT_FORBIDDEN;
    assert(xl6ArrayResizeV1(value,LAYOUT,1) == defaultable);
    if (!defaultable) {
        assert(xrtGetError()); xrtClearError();
        assert(xl6ArraySizeV1(value,LAYOUT,false) == 0);
    } else {
        assert(!xrtGetError() && xl6ArraySizeV1(value,LAYOUT,false) == 1);
        if (plan->abi == XL6_IR_ARRAY_LAYOUT_NULLABLE_C_SCALAR_V1) {
            union { max_align_t alignment; unsigned char bytes[64]; } slot;
            assert(physical->Size<=sizeof(slot.bytes));
            memset(slot.bytes,0xA5,sizeof(slot.bytes));
            xl6ArrayReadV1(value,LAYOUT,0,slot.bytes);
            assert(!xrtGetError() && slot.bytes[0]==0); /* No absent payload read. */
        }
        if (plan->abi == XL6_IR_ARRAY_LAYOUT_REFERENCE_V2 ||
            plan->abi == XL6_IR_ARRAY_LAYOUT_CALLABLE_REFERENCE_V1 ||
            plan->abi == XL6_IR_ARRAY_LAYOUT_SEQUENCE_REFERENCE_V1) {
            xvalue* item=NULL; xl6ArrayReadV1(value,LAYOUT,0,&item);
            assert(!xrtGetError());
            if (plan->defaultAbi == XL6_IR_ARRAY_DEFAULT_EMPTY_BYTES) {
                xbytesview bytes;
                assert(item && xrtValueGetBytes(item,&bytes) && !bytes.Size);
            } else assert(!item || xrtValueType(item)==XVALUE_NULL);
            xrtValueRelease(item);
        }
    }
    assert(xl6ArrayClearV1(value,LAYOUT) && !xrtGetError());
    xrtValueRelease(value); assert(!xrtGetError());
#undef LAYOUT
    return 1;
}
static void layouts(XL6_IrModule* ir)
{
    unsigned arrays=0, resident=0, mutations=0;
    for (size_t i=0;i<ir->types.count;++i) {
        XL6_IrType* type=&ir->types.items[i];
        XL6_IrCollectionCarrierAbi carrier=type->collectionCarrierAbi;
        XL6_IrArrayLayout saved=type->arrayLayout;
        assert(carrier == (type->collectionAbi == XL6_IR_COLLECTION_NONE
            ? XL6_IR_COLLECTION_CARRIER_NONE : XL6_IR_COLLECTION_CARRIER_VALUE_V1));
        type->collectionCarrierAbi=carrier == XL6_IR_COLLECTION_CARRIER_NONE
            ? XL6_IR_COLLECTION_CARRIER_VALUE_V1 : XL6_IR_COLLECTION_CARRIER_NONE;
        rejected(ir); ++mutations;
        type->collectionCarrierAbi=XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1; rejected(ir); ++mutations;
        type->collectionCarrierAbi=(XL6_IrCollectionCarrierAbi)UINT32_MAX; rejected(ir); ++mutations;
        type->collectionCarrierAbi=carrier;
#define MUTATE(FIELD, VALUE) do { type->arrayLayout.FIELD=(VALUE); rejected(ir); \
    type->arrayLayout=saved; ++mutations; } while (0)
        MUTATE(abi,(XL6_IrArrayLayoutAbi)UINT32_MAX);
        MUTATE(elementType,saved.elementType ? XL6_IR_TYPE_INVALID : (XL6_IrTypeId)1);
        MUTATE(baseElementType,saved.baseElementType ? XL6_IR_TYPE_INVALID : (XL6_IrTypeId)1);
        MUTATE(slotAbi,(XL6_IrNativeSlotAbi)UINT32_MAX);
        MUTATE(codecAbi,(XL6_IrArrayCodecAbi)UINT32_MAX);
        MUTATE(transferAbi,(XL6_IrRuntimeResourcePayloadAbi)UINT32_MAX);
        MUTATE(admissionAbi,(XL6_ValueAdmissionV1)UINT32_MAX);
        MUTATE(nullable,!saved.nullable);
        MUTATE(defaultAbi,(XL6_IrArrayDefaultAbi)UINT32_MAX);
#undef MUTATE
        if (type->collectionAbi != XL6_IR_COLLECTION_ARRAY) {
            assert(saved.abi == XL6_IR_ARRAY_LAYOUT_NONE);
            continue;
        }
        ++arrays;
        if (!type->paramCount) {
            assert(saved.abi == XL6_IR_ARRAY_LAYOUT_DYNAMIC_VALUE && !saved.elementType &&
                !saved.baseElementType && !saved.slotAbi && !saved.transferAbi && !saved.admissionAbi &&
                !saved.codecAbi && !saved.nullable && !saved.defaultAbi);
            continue;
        }
        assert(type->paramCount == 1 && saved.elementType == type->params[0]);
        const XL6_IrType* element=xl6IrTypeGet(&ir->types,saved.elementType);
        assert(element);
        if (saved.abi==XL6_IR_ARRAY_LAYOUT_FUTURE_REFERENCE_V1) {
            assert(!element->nativeSlotAbi && saved.slotAbi==XL6_IR_NATIVE_SLOT_FUTURE_PTR &&
                saved.codecAbi==XL6_IR_ARRAY_CODEC_DIRECT && saved.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF);
            XL6_IrType* future=&ir->types.items[saved.baseElementType-1];
            XL6_IrTypeId* parameters=future->params; future->params=NULL;
            XL6_IrArrayLayout derived=xl6IrArrayLayoutForType(&ir->types,type);
            assert(derived.abi==XL6_IR_ARRAY_LAYOUT_PENDING_SLOT && !derived.codecAbi);
            rejected(ir); ++mutations; future->params=parameters;
        } else if (saved.abi==XL6_IR_ARRAY_LAYOUT_CALLABLE_REFERENCE_V1) {
            assert(!element->nativeSlotAbi && saved.slotAbi==XL6_IR_NATIVE_SLOT_XVALUE_CARRIER &&
                saved.codecAbi==XL6_IR_ARRAY_CODEC_CLOSURE_VALUE_V1);
            type->arrayLayout.codecAbi=XL6_IR_ARRAY_CODEC_DIRECT; rejected(ir); ++mutations;
            type->arrayLayout=saved;
            XL6_IrType* signature=&ir->types.items[saved.baseElementType-1];
            if (signature->paramCount) {
                XL6_IrTypeId* parameters=signature->params; signature->params=NULL;
                XL6_IrArrayLayout derived=xl6IrArrayLayoutForType(&ir->types,type);
                assert(derived.abi==XL6_IR_ARRAY_LAYOUT_PENDING_SLOT && !derived.codecAbi);
                rejected(ir); ++mutations; signature->params=parameters;
            }
        } else {
            assert(saved.slotAbi==element->nativeSlotAbi);
            assert(saved.codecAbi==(saved.defaultAbi ? XL6_IR_ARRAY_CODEC_DIRECT : XL6_IR_ARRAY_CODEC_NONE));
        }
        assert(saved.nullable == (element->kind == XL6_IR_TYPE_NULLABLE));
        assert(saved.baseElementType == (saved.nullable ? element->params[0] : saved.elementType));
        if (saved.abi==XL6_IR_ARRAY_LAYOUT_NUMERIC_ENUM_V1) {
            const XL6_IrType* base=xl6IrTypeGet(&ir->types,saved.baseElementType);
            assert(base && base->kind==XL6_IR_TYPE_NUMERIC_ENUM && !element->xvalueAdmissionAbi);
            static const XL6_ValueAdmissionV1 rules[]={XL6_VALUE_ADMIT_NONE,
                XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_U8,XL6_VALUE_ADMIT_I16,XL6_VALUE_ADMIT_U16,
                XL6_VALUE_ADMIT_I32,XL6_VALUE_ADMIT_U32,XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_U64};
            assert(base->integerTarget>0 && base->integerTarget<sizeof(rules)/sizeof(rules[0]) &&
                saved.admissionAbi==rules[base->integerTarget]);
        } else assert(saved.admissionAbi == element->xvalueAdmissionAbi);
        if (saved.abi==XL6_IR_ARRAY_LAYOUT_SEQUENCE_REFERENCE_V1) {
            XL6_IrType* base=&ir->types.items[saved.baseElementType-1];
            assert(base->collectionAbi==XL6_IR_COLLECTION_ARRAY);
            XL6_IrArrayLayout childSaved=base->arrayLayout;
            base->arrayLayout.abi=(XL6_IrArrayLayoutAbi)UINT32_MAX;
            XL6_IrArrayLayout derived=xl6IrArrayLayoutForType(&ir->types,type);
            assert(xl6IrArrayLayoutEqual(&derived,&saved)); /* Never trust child cache. */
            base->arrayLayout=childSaved;
            if (base->paramCount==1) {
                XL6_IrTypeId parameter=base->params[0];
                base->params[0]=saved.baseElementType; /* Malformed self-recursive type graph. */
                derived=xl6IrArrayLayoutForType(&ir->types,type);
                assert(derived.abi==XL6_IR_ARRAY_LAYOUT_PENDING_DOMAIN);
                rejected(ir); ++mutations;
                base->params[0]=parameter;
            }
        }
        resident+=residentRecipe(ir,type);
    }
    assert(xl6IrVerifyModule(ir,NULL));
    printf("array layout: %u arrays; %u real resident recipes; %u carrier/layout mutations refused before C/header output\n",
        arrays,resident,mutations);
}
static XL6_IrInstruction* definition(XL6_IrModule* ir, XL6_IrValueId value)
{
    for (size_t i=0;i<ir->instructionCount;++i)
        if (ir->instructions[i].result==value && value!=XL6_IR_VALUE_INVALID)
            return &ir->instructions[i];
    return NULL;
}

static void numericEnums(XL6_IrModule* ir)
{
    unsigned binds=0, nullableBoxes=0, mutations=0;
    for (size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* bind=&ir->instructions[i];
        if (bind->runtimeOp!=XL6_IR_RUNTIME_XVALUE_TYPE_BIND) continue;
        const XL6_IrType* identity=xl6IrTypeGet(&ir->types,bind->runtimeIdentityType);
        if (!identity || identity->kind!=XL6_IR_TYPE_NUMERIC_ENUM) continue;
        assert(bind->operandCount==1);
        XL6_IrInstruction* box=definition(ir,ir->operands[bind->firstOperand]);
        assert(box && box->operandCount==1 && box->kind==XL6_IR_INSTR_RUNTIME_CALL);
        XL6_IrInstruction* scalar=definition(ir,ir->operands[box->firstOperand]);
        assert(scalar && scalar->type==bind->runtimeIdentityType);
        assert(box->runtimeOp==(identity->cAbi==XL6_IR_C_ABI_U64
            ? XL6_IR_RUNTIME_XVALUE_U64 : XL6_IR_RUNTIME_XVALUE_I64));
        assert(xl6IrCallInstructionErrorAbi(ir,box)==XL6_IR_ERROR_ABI_MAY_RAISE);
        assert(xl6IrCallInstructionErrorPhysicalAbi(ir,box)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
        XL6_IrTypeId saved=bind->runtimeIdentityType;
        bind->runtimeIdentityType=XL6_IR_TYPE_INVALID; rejected(ir); ++mutations;
        unsigned otherEnums=0, otherScalars=0;
        for (size_t t=0;t<ir->types.count;++t) {
            const XL6_IrType* other=&ir->types.items[t];
            if ((XL6_IrTypeId)(t+1)==saved) continue;
            /* The contract is exact TypeId equality; one peer and one raw
             * scalar witness suffice for each actual producer. Do not repeat
             * the same refusal for every unrelated type in the module. */
            if (other->kind==XL6_IR_TYPE_NUMERIC_ENUM) {
                if (otherEnums++) continue;
            } else if (other->kind==XL6_IR_TYPE_BUILTIN && other->cAbi==XL6_IR_C_ABI_I64) {
                if (otherScalars++) continue;
            } else continue;
            bind->runtimeIdentityType=(XL6_IrTypeId)(t+1); rejected(ir); ++mutations;
        }
        bind->runtimeIdentityType=saved;
        XL6_IrRuntimeOp operation=box->runtimeOp;
        box->runtimeOp=operation==XL6_IR_RUNTIME_XVALUE_U64
            ? XL6_IR_RUNTIME_XVALUE_I64 : XL6_IR_RUNTIME_XVALUE_U64;
        rejected(ir); ++mutations; box->runtimeOp=operation;
        if (scalar->kind==XL6_IR_INSTR_NULLABLE_UNWRAP) {
            ++nullableBoxes;
            assert(scalar->operandCount==1);
            XL6_IrValueId operand=ir->operands[box->firstOperand];
            ir->operands[box->firstOperand]=ir->operands[scalar->firstOperand];
            box->runtimeOp=XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX;
            rejected(ir); ++mutations;
            ir->operands[box->firstOperand]=operand; box->runtimeOp=operation;
        }
        assert(xl6IrVerifyModule(ir,NULL)); ++binds;
    }
    if (binds) printf("numeric enum bridge: %u binds; %u nullable boxes; %u rejected mutations before C/header output\n",
        binds,nullableBoxes,mutations);
}

static void futureProjectionContracts(XL6_IrModule* ir)
{
    unsigned projections = 0, mutations = 0;
    for (size_t i = 0; i < ir->instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir->instructions[i];
        XL6_IrRuntimeOp operation = instruction->runtimeOp;
        if (operation != XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY &&
            operation != XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_FUTURE_COPY) continue;
        XL6_IrTypeId payload = instruction->payloadType;
        assert(payload == xl6IrFutureProjectionPayloadType(ir, instruction->type, operation));
        instruction->payloadType = XL6_IR_TYPE_INVALID; rejected(ir); ++mutations;
        instruction->payloadType = instruction->type; rejected(ir); ++mutations;
        instruction->payloadType = payload;
        instruction->runtimeOp = operation == XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY
            ? XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_FUTURE_COPY : XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY;
        rejected(ir); ++mutations; instruction->runtimeOp = operation;
        XL6_IrTypeId callee = instruction->calleeType;
        instruction->calleeType = instruction->type; rejected(ir); ++mutations;
        instruction->calleeType = callee;
        XL6_IrTypeId identity = instruction->runtimeIdentityType;
        instruction->runtimeIdentityType = instruction->type; rejected(ir); ++mutations;
        instruction->runtimeIdentityType = identity;
        assert(xl6IrVerifyModule(ir, NULL)); ++projections;
    }
    if (projections) printf("Future projection contracts: %u projections; %u payload/nullability/metadata mutations refused before C/header output\n",
        projections, mutations);
}

static void futureContainerSemantics(void)
{
    const struct { const char* source; bool accepted; } cases[] = {
        {"func good(array<future<int>> values)\n auto item = values[0]\nend\n", true},
        {"func good(array<future<int>?> values)\n auto item = values[0]\nend\n", true},
        {"typedef F = future<int>\ntypedef MaybeF = F?\n"
         "func good(array<MaybeF> values)\n auto item = values[0]\nend\n", true},
        {"func good(list<future<int>?> values)\n auto item = values[0]\nend\n", true},
        {"func good(dict<future<int>?> values)\n auto item = values[\"a\"]\nend\n", true},
        {"func bad(future<int> value)\n auto copy = value\nend\n", false},
        {"func bad(future<int>? value)\n auto copy = value\nend\n", false},
        {"typedef F = future<int>\nfunc bad(F value)\n auto copy = value\nend\n", false},
        {"func bad(array<mutex?> values)\nend\n", false},
        {"func bad(array<generator<int>?> values)\nend\n", false},
        {"func bad(list<mutex?> values)\nend\n", false},
        {"func bad(dict<mutex?> values)\nend\n", false}
    };
    unsigned accepted = 0, rejectedCount = 0;
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        XL6_ParseResult parse; xl6ParseResultInit(&parse);
        assert(xl6ParseSource(cases[i].source, "future-container-semantics", &parse));
        assert(xl6StdlibExpandModule(parse.root, &parse.diags));
        bool ok = xl6SemCheckModule(parse.root, &parse.diags, "future_container_semantics") != 0;
        if (ok != cases[i].accepted) {
            fprintf(stderr, "Future container semantics case %zu failed\n", i);
            for (size_t d = 0; d < parse.diags.count; ++d)
                fprintf(stderr, "%s\n", parse.diags.items[d].message);
            abort();
        }
        if (ok) { assert(!parse.diags.count); ++accepted; }
        else { assert(parse.diags.count); ++rejectedCount; }
        xl6ParseResultFree(&parse);
    }
    printf("Future container semantics: %u accepted; %u independent copy/resource refusals\n",
        accepted, rejectedCount);
}

int main(int argc, char** argv)
{
    assert(argc==3 || (argc==4 && strcmp(argv[3],"numeric-enums")==0));
    futureContainerSemantics();
    int ok=0; unsigned checked=0, unrelated=0;
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output output;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"collection_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    futureProjectionContracts(&ir);
    numericEnums(&ir);
    if (argc==3) layouts(&ir);
    unsigned defaults=0;
    for (size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        XL6_ValueDefaultAbiV1 saved=instruction->collectionDefaultAbi;
        if (instruction->kind==XL6_IR_INSTR_RUNTIME_CALL &&
            instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
            instruction->typedCollectionOp==XL6_IR_TYPED_COLLECTION_RESIZE) {
            assert(saved!=XL6_VALUE_DEFAULT_INVALID);
            assert(xl6IrCallInstructionErrorAbi(&ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE);
            instruction->collectionDefaultAbi=XL6_VALUE_DEFAULT_INVALID; rejected(&ir);
            instruction->collectionDefaultAbi=(XL6_ValueDefaultAbiV1)UINT32_MAX; rejected(&ir);
            instruction->collectionDefaultAbi=saved==XL6_VALUE_DEFAULT_NULL ? XL6_VALUE_DEFAULT_INT : XL6_VALUE_DEFAULT_NULL;
            rejected(&ir); instruction->collectionDefaultAbi=saved;
            XL6_IrInstruction* test=&ir.instructions[i+1];
            assert(test->kind==XL6_IR_INSTR_ERROR_TEST);
            XL6_IrInstructionKind kind=test->kind; test->kind=XL6_IR_INSTR_INVALID; rejected(&ir); test->kind=kind;
            ++defaults;
        } else {
            assert(saved==XL6_VALUE_DEFAULT_INVALID);
            if (i==0) { instruction->collectionDefaultAbi=XL6_VALUE_DEFAULT_INT; rejected(&ir);
                instruction->collectionDefaultAbi=saved; }
        }
    }
    for (size_t i=0;i<ir.intrinsicCallCount;++i) {
        XL6_IrIntrinsicCall* intrinsic=&ir.intrinsicCalls[i];
        if (intrinsic->kind==XL6_IR_INTRINSIC_TYPED_COLLECTION_MEMBER &&
            intrinsic->typedCollectionOp==XL6_IR_TYPED_COLLECTION_RESIZE) {
            assert(intrinsic->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
            intrinsic->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE; rejected(&ir);
            intrinsic->errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE;
        }
    }
    for (size_t i=0;i<ir.instructionCount;++i) {
        XL6_IrInstruction* instruction=&ir.instructions[i];
        if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
            instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND) {
            assert(!instruction->collectionAdmissionAbi && !instruction->collectionAdmissionIdentitySet &&
                !instruction->collectionAdmissionNullable);
            if (!unrelated) {
                instruction->collectionAdmissionAbi=XL6_IR_COLLECTION_ADMISSION_PRIMITIVE;
                rejected(&ir); instruction->collectionAdmissionAbi=XL6_IR_COLLECTION_ADMISSION_NONE;
                instruction->collectionAdmissionIdentitySet=1;
                rejected(&ir); instruction->collectionAdmissionIdentitySet=0;
                instruction->collectionAdmissionNullable=true;
                rejected(&ir); instruction->collectionAdmissionNullable=false;
                ++unrelated;
            }
            continue;
        }
        XL6_IrCollectionAdmissionAbi family=instruction->collectionAdmissionAbi;
        XL6_IrNominalIdentitySetId identitySet=instruction->collectionAdmissionIdentitySet;
        bool nullable=instruction->collectionAdmissionNullable;
        instruction->collectionAdmissionAbi=XL6_IR_COLLECTION_ADMISSION_NONE; rejected(&ir);
        instruction->collectionAdmissionAbi=(XL6_IrCollectionAdmissionAbi)0x7fffffff; rejected(&ir);
        instruction->collectionAdmissionAbi=family==XL6_IR_COLLECTION_ADMISSION_PRIMITIVE
            ? XL6_IR_COLLECTION_ADMISSION_NOMINAL : XL6_IR_COLLECTION_ADMISSION_PRIMITIVE;
        rejected(&ir); instruction->collectionAdmissionAbi=family;
        instruction->collectionAdmissionIdentitySet=identitySet ? 0 : 1; rejected(&ir);
        instruction->collectionAdmissionIdentitySet=UINT32_MAX; rejected(&ir);
        instruction->collectionAdmissionIdentitySet=identitySet;
        instruction->collectionAdmissionNullable=!nullable; rejected(&ir);
        instruction->collectionAdmissionNullable=nullable;
        assert(xl6IrVerifyModule(&ir,NULL)); ++checked;
    }
    assert(unrelated && xl6IrVerifyModule(&ir,NULL));
    if (!xl6OutputOpenFile(&output,argv[2])) goto done;
    ok=xl6IrCgenEmitC(&output,&ir,&parse.diags); xl6OutputClose(&output);
    printf("collection contract: %u checked rebinds; family/set/nullable and unrelated fields refused before C/header output\n",checked);
    if (defaults) printf("array defaults: %u resize contracts; default/effect/immediate outcome refused before C/header output\n",defaults);
done:
    for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%d:%d: %s\n",parse.diags.items[i].line,
        parse.diags.items[i].column,parse.diags.items[i].message);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return !ok;
}
