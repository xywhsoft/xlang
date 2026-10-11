#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static unsigned mutations;
static void refused(const XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_NATIVE_VALUE",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && fixture_contract(argv[1])); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL)); unsigned operations[4]={0};
    for (size_t i=0;i<ir->types.count;++i) {
        XL6_IrType* type=&ir->types.items[i];
        if (type->cAbi != XL6_IR_C_ABI_NATIVE_VALUE) continue;
        assert(xl6IrNativeValuePlanForType(type));
        assert(xl6IrTypeHasStorageReleaseAbi(ir,(XL6_IrTypeId)(i+1)) &&
            xl6IrTypeReleaseEffect(ir,(XL6_IrTypeId)(i+1))==XL6_IR_RELEASE_EFFECT_NO_RAISE);
        XL6_IrType saved=*type;
#define BAD_TYPE(Field,Value) do { type->Field=(Value); refused(ir); *type=saved; } while (0)
        BAD_TYPE(nativeValueAbi,XL6_IR_NATIVE_VALUE_NONE);
        BAD_TYPE(nativeValueAbi,(XL6_IrNativeValueAbi)99);
        BAD_TYPE(cAbi,XL6_IR_C_ABI_PRODUCT); BAD_TYPE(cAbi,XL6_IR_C_ABI_I64);
        BAD_TYPE(ownershipAbi,XL6_IR_OWNERSHIP_ABI_TRIVIAL);
        BAD_TYPE(nullAbi,XL6_IR_NULL_ABI_POINTER_SENTINEL);
        BAD_TYPE(abiId,0); BAD_TYPE(kind,XL6_IR_TYPE_CLASS);
        BAD_TYPE(nativeSlotAbi,XL6_IR_NATIVE_SLOT_XVALUE_CARRIER);
        BAD_TYPE(paramCount,1);
#undef BAD_TYPE
    }
    for (size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];
        if (ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || ins->runtimeOp<XL6_IR_RUNTIME_NATIVE_VALUE_COPY ||
            ins->runtimeOp>XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED) continue;
        assert(xl6IrNativeValueInstructionPlan(ir,ins));
        for (size_t p=0;p<ins->operandCount;++p)
            assert(xl6IrInstructionOperandOwnership(ir,ins,p)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        ++operations[ins->runtimeOp-XL6_IR_RUNTIME_NATIVE_VALUE_COPY];
        if (ins->runtimeOp!=XL6_IR_RUNTIME_NATIVE_VALUE_EQUAL) {
            assert(xl6IrCallInstructionErrorAbi(ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE);
            assert(xl6IrCallInstructionErrorPhysicalAbi(ir,ins)==
                (ins->runtimeOp==XL6_IR_RUNTIME_NATIVE_VALUE_BOX ?
                    XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT : XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT));
            assert(xl6IrValueOwnershipGet(ir,ins->result)==XL6_IR_OWNERSHIP_MOVE);
        }
        XL6_IrInstruction saved=*ins;
#define BAD(Field,Value) do { ins->Field=(Value); refused(ir); *ins=saved; } while (0)
        BAD(type,XL6_IR_TYPE_INVALID); BAD(firstOperand,SIZE_MAX); BAD(operandCount,0);
        BAD(result,XL6_IR_VALUE_INVALID); BAD(runtimeIdentityType,saved.type);
        BAD(targetSymbol,UINT32_MAX); BAD(immediateText,(char*)"erased");
        BAD(immediateData,(unsigned char*)"binary"); BAD(immediateSize,1);
        BAD(opcode,XL6_IR_OPCODE_ADD); BAD(calleeType,saved.type);
        if (saved.runtimeOp==XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED) BAD(castAbi,XL6_IR_CAST_INVALID);
        else BAD(castAbi,XL6_IR_CAST_XVALUE_TO_NATIVE_VALUE_CHECKED);
        if (saved.runtimeOp!=XL6_IR_RUNTIME_NATIVE_VALUE_COPY) BAD(payloadType,XL6_IR_TYPE_INVALID);
#undef BAD
        if (saved.runtimeOp==XL6_IR_RUNTIME_NATIVE_VALUE_COPY) {
            ins->kind=XL6_IR_INSTR_RETAIN; ins->runtimeOp=XL6_IR_RUNTIME_NONE; refused(ir); *ins=saved;
        }
        if (saved.runtimeOp!=XL6_IR_RUNTIME_NATIVE_VALUE_EQUAL) {
            assert(i+1<ir->instructionCount && ir->instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
            XL6_IrInstruction outcome=ir->instructions[i+1];
            ir->instructions[i+1].kind=XL6_IR_INSTR_VALUE; refused(ir); ir->instructions[i+1]=outcome;
            ir->instructions[i+1].trueTarget=outcome.falseTarget; refused(ir); ir->instructions[i+1]=outcome;
        }
    }
    for (unsigned i=0;i<4;++i) assert(operations[i]);
    assert(xl6IrVerifyModule(ir,NULL));
    printf("native values copy=%u equal=%u box=%u project=%u mutations=%u\n",
        operations[0],operations[1],operations[2],operations[3],mutations);
    xl6CompileUnitFree(&unit); return 0;
}
