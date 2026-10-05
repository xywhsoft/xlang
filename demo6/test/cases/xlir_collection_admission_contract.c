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
        plan->abi != XL6_IR_ARRAY_LAYOUT_SEQUENCE_REFERENCE_V1) return 0;
    const XL6_IrType* element=xl6IrTypeGet(&ir->types,plan->elementType);
    const XL6_IrType* base=xl6IrTypeGet(&ir->types,plan->baseElementType);
    assert(element && base && type->typeValueId && element->typeValueId && base->typeValueId);
    const xrttype* physical=descriptor(plan->slotAbi);
    assert(physical);
    xvalue* value=NULL;
    if (plan->abi == XL6_IR_ARRAY_LAYOUT_SEQUENCE_REFERENCE_V1) {
        assert(base->collectionAbi==XL6_IR_COLLECTION_ARRAY &&
            plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN && !plan->admissionAbi);
        value=xl6ArraySequenceCreateV1(type->typeValueId,element->typeValueId,base->typeValueId,plan->nullable);
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
                !saved.nullable && !saved.defaultAbi);
            continue;
        }
        assert(type->paramCount == 1 && saved.elementType == type->params[0]);
        const XL6_IrType* element=xl6IrTypeGet(&ir->types,saved.elementType);
        assert(element && saved.slotAbi == element->nativeSlotAbi);
        assert(saved.nullable == (element->kind == XL6_IR_TYPE_NULLABLE));
        assert(saved.baseElementType == (saved.nullable ? element->params[0] : saved.elementType));
        assert(saved.admissionAbi == element->xvalueAdmissionAbi);
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
int main(int argc, char** argv)
{
    assert(argc==3); int ok=0; unsigned checked=0, unrelated=0;
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output output;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"collection_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrLowerModule(&ir,&parse.diags)) goto done;
    assert(xl6IrVerifyModule(&ir,NULL));
    layouts(&ir);
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
