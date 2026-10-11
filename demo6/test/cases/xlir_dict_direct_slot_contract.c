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
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"REJECTED_DIRECT_SLOT",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && fixture_contract(argv[1])); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir; assert(xl6IrVerifyModule(ir,NULL)); unsigned reads=0,writes=0;
    for (size_t i=0;i<ir->instructionCount;++i) {
        XL6_IrInstruction* ins=&ir->instructions[i];
        if (ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !xl6IrDictDirectSlotRuntimeOpIs(ins->runtimeOp)) continue;
        bool write=ins->runtimeOp==XL6_IR_RUNTIME_DICT_SET_SLOT;
        assert(xl6IrDictDirectSlotRuntimeInstructionCompatible(ir,ins));
        assert(xl6IrCallInstructionErrorAbi(ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE &&
            xl6IrCallInstructionErrorPhysicalAbi(ir,ins)==XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE);
        for (size_t operand=0;operand<ins->operandCount;++operand)
            assert(xl6IrInstructionOperandOwnership(ir,ins,operand)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
        XL6_IrInstruction saved=*ins;
#define BAD(Field,Value) do { ins->Field=(Value); refused(ir); *ins=saved; } while (0)
        BAD(type,XL6_IR_TYPE_INVALID); BAD(firstOperand,SIZE_MAX); BAD(operandCount,0);
        BAD(payloadType,saved.type); BAD(runtimeIdentityType,saved.type); BAD(targetSymbol,UINT32_MAX);
        BAD(immediateText,(char*)"erased"); BAD(sequenceAdmissionIdentitySet,UINT32_MAX);
        BAD(collectionAbi,XL6_IR_COLLECTION_DICT); BAD(opcode,XL6_IR_OPCODE_ADD);
#undef BAD
        if (write) ++writes; else ++reads;
    }
    assert(reads && writes && xl6IrVerifyModule(ir,NULL));
    printf("direct slot reads=%u writes=%u mutations=%u\n",reads,writes,mutations);
    xl6CompileUnitFree(&unit); return 0;
}
