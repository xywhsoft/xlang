#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned mutations;
static void refused(const XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_NAMED_ABI",NULL) : xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size);
        xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==5);
    unsigned reads=0,writes=0;
    for(unsigned lane=0;lane<4;++lane) {
        XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[lane+1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* module=&unit.ir; assert(xl6IrVerifyModule(module,NULL));
        for(size_t i=0;i<module->instructionCount;++i) {
            XL6_IrInstruction* instruction=&module->instructions[i];
            bool read=instruction->runtimeOp==XL6_IR_RUNTIME_NAMED_READ_LITERAL_COPY;
            bool write=instruction->runtimeOp==XL6_IR_RUNTIME_NAMED_SET_LITERAL_TAKE;
            if(!read && !write) continue;
            assert(lane<2 && xl6IrNamedValueInstructionCompatible(module,instruction));
            assert(xl6IrCallInstructionErrorAbi(module,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE &&
                xl6IrCallInstructionErrorPhysicalAbi(module,instruction)==(read ?
                    XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT : XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE));
            assert(xl6IrInstructionOperandOwnership(module,instruction,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
            if(write) assert(xl6IrInstructionOperandOwnership(module,instruction,1)==XL6_IR_OPERAND_OWNERSHIP_CONSUME);
            else assert(xl6IrValueOwnershipGet(module,instruction->result)==XL6_IR_OWNERSHIP_MOVE);
            assert(i+1<module->instructionCount && module->instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
            XL6_IrInstruction saved=*instruction;
#define MUTATE(Field,Value) do { instruction->Field=(Value); refused(module); *instruction=saved; } while(0)
            MUTATE(type,0); MUTATE(result,0); MUTATE(operandCount,0); MUTATE(firstOperand,SIZE_MAX);
            MUTATE(targetValue,saved.result); MUTATE(targetSymbol,UINT32_MAX); MUTATE(calleeType,saved.type);
            MUTATE(payloadType,saved.type); MUTATE(runtimeIdentityType,saved.type); MUTATE(opcode,XL6_IR_OPCODE_ADD);
            MUTATE(immediateText,(char*)"not-a-literal"); MUTATE(immediateText,NULL); MUTATE(immediateSize,1);
            MUTATE(immediateText,(char*)"\"wrong-key\"");
            MUTATE(typedCollectionOp,XL6_IR_TYPED_COLLECTION_CLEAR);
            MUTATE(dynamicCollectionOp,XL6_IR_DYNAMIC_COLLECTION_CLEAR);
#undef MUTATE
            for(size_t n=0;n<saved.operandCount;++n) {
                XL6_IrValueId* value=&module->operands[saved.firstOperand+n],old=*value;
                *value=0; refused(module); *value=old;
            }
            XL6_IrInstruction* test=&module->instructions[i+1]; XL6_IrInstruction prior=*test;
            test->kind=XL6_IR_INSTR_INVALID; refused(module); *test=prior;
            if(read) ++reads; else ++writes;
        }
        assert(xl6IrVerifyModule(module,NULL));
        XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(module,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
        if(lane==1) {
            assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_NAMED_READ) &&
                xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_NAMED_WRITE));
            const XL6_ArtifactRuntimeId excluded[]={XL6_ARTIFACT_VALUE_INDEX_READ,XL6_ARTIFACT_VALUE_INDEX_WRITE,
                XL6_ARTIFACT_ARRAY_ERASED,XL6_ARTIFACT_LIST_NATIVE,XL6_ARTIFACT_SET_NATIVE};
            for(size_t n=0;n<sizeof(excluded)/sizeof(excluded[0]);++n)
                assert(!xl6ArtifactRuntimeSetHas(&plan.components,excluded[n]));
        } else if(lane>=2) {
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_NAMED_READ) &&
                !xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_NAMED_WRITE));
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_NATIVE));
        }
        xl6CompileUnitFree(&unit);
    }
    assert(reads>10 && writes>5 && mutations>100);
    printf("named record contract reads=%u writes=%u mutations=%u; owned ABI and class/numeric pruning verified\n",reads,writes,mutations);
    return 0;
}
