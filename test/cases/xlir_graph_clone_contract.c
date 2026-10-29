#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static unsigned corrupt,operations,isolations;
static void refuse(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_GRAPH_CLONE",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* bytes=xl6OutputTakeMemoryText(&out,&size); assert(size==0); xrtFree(bytes); xl6OutputDiscard(&out);
    }
    ++corrupt;
}
int main(int argc,char** argv)
{
    assert(argc==5);
    for (unsigned lane=1;lane<5;++lane) {
        assert(fixture_contract(argv[lane])); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[lane],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* ir=&unit.ir; XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_GRAPH));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_GRAPH)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_GRAPH)==(lane==1));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_COPY_GRAPH));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_COPY_SET_GRAPH));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_COPY_COLLECTIONS));
        if (lane!=1) {
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE));
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_NATIVE));
        }
        for (size_t i=0;i<ir->instructionCount;++i) {
            XL6_IrInstruction* instruction=&ir->instructions[i];
            if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL || instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_DEEP_CLONE) continue;
            assert(instruction->copyGraphProviders&XL6_IR_GRAPH_DICT_V1);
            assert(xl6IrValueCopyProviderAbi(ir,instruction)==XL6_IR_VALUE_COPY_NATIVE_GRAPH_V1);
            assert(xl6IrRuntimeOperandOwnership(instruction->runtimeOp,0)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
            assert(xl6IrCallInstructionErrorAbi(ir,instruction)==XL6_IR_ERROR_ABI_MAY_RAISE &&
                xl6IrCallInstructionErrorPhysicalAbi(ir,instruction)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
            XL6_IrInstruction original=*instruction;
            instruction->copyGraphProviders=0; refuse(ir); *instruction=original;
            instruction->copyGraphProviders^=XL6_IR_GRAPH_DICT_V1; refuse(ir); *instruction=original;
            instruction->copyGraphProviders|=8; refuse(ir); *instruction=original;
            instruction->runtimeOp=XL6_IR_RUNTIME_XVALUE_CLONE; refuse(ir); *instruction=original;
            instruction->operandCount=0; refuse(ir); *instruction=original;
            instruction->type=XL6_IR_TYPE_INVALID; refuse(ir); *instruction=original;
            assert(i+1<ir->instructionCount); XL6_IrInstruction* test=&ir->instructions[i+1];
            assert(test->kind==XL6_IR_INSTR_ERROR_TEST); XL6_IrInstruction saved=*test;
            test->falseTarget=test->trueTarget; refuse(ir); *test=saved;
            ++operations;
        }
        for (size_t i=0;i<ir->persistenceRestoreNodeCount;++i) {
            XL6_IrPersistenceRestoreNode* node=&ir->persistenceRestoreNodes[i];
            if (!node->isolationGraphProviders) continue;
            unsigned original=node->isolationGraphProviders;
            assert(node->isolationAbi==XL6_IR_PERSIST_ISOLATION_RUNTIME_DEEP);
            node->isolationGraphProviders=0; refuse(ir); node->isolationGraphProviders=original;
            node->isolationGraphProviders^=XL6_IR_GRAPH_DICT_V1; refuse(ir); node->isolationGraphProviders=original;
            ++isolations;
        }
        xl6CompileUnitFree(&unit);
    }
    assert(operations>=10 && isolations>=2 && corrupt>=74);
    printf("Graph clone contracts: operations=%u isolations=%u corrupt=%u\n",operations,isolations,corrupt);
    return 0;
}
