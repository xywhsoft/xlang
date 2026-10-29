#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static unsigned mutations;
static void refuse(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_DICT_IMPORT",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(!size); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==3); unsigned imports=0,restores=0;
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_DICT_IMPORT_CHAIN,0)==XL6_IR_OPERAND_OWNERSHIP_CONSUME);
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_DICT_IMPORT_CHAIN,1)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
    for (unsigned lane=1;lane<3;++lane) {
        assert(fixture_contract(argv[lane])); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[lane],1));
        if (!xl6IrLowerModule(&unit.ir,&unit.parse.diags)) { xl6DiagPrintText(&unit.parse.diags,argv[lane]); return 1; }
        XL6_IrModule* ir=&unit.ir; XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_IMPORT_CHAIN));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_NATIVE));
        if (lane==2) {
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE));
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_NATIVE));
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_SCHEMA));
        }
        for (size_t i=0;i<ir->instructionCount;++i) {
            XL6_IrInstruction* ins=&ir->instructions[i];
            if (ins->runtimeOp!=XL6_IR_RUNTIME_DICT_IMPORT_CHAIN) continue;
            ++imports; assert(xl6IrDictImportRuntimeInstructionCompatible(ir,ins));
            assert(xl6IrCallInstructionErrorAbi(ir,ins)==XL6_IR_ERROR_ABI_MAY_RAISE);
            assert(xl6IrCallInstructionErrorPhysicalAbi(ir,ins)==XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT);
            assert(xl6IrValueOwnershipGet(ir,ins->result)==XL6_IR_OWNERSHIP_MOVE);
            XL6_IrInstruction saved=*ins;
#define BAD(Member,Value) do { ins->Member=Value; refuse(ir); *ins=saved; } while(0)
            BAD(operandCount,0); BAD(collectionAbi,XL6_IR_COLLECTION_LIST); BAD(payloadType,ins->type);
            BAD(runtimeIdentityType,ins->type); BAD(immediateText,(char*)"wire"); BAD(targetValue,ins->result);
            BAD(sequenceAdmissionIdentitySet,UINT32_MAX); BAD(collectionAdmissionAbi,XL6_IR_COLLECTION_ADMISSION_PRIMITIVE);
            BAD(runtimeOp,XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND);
#undef BAD
            XL6_IrType* type=&ir->types.items[ins->type-1]; XL6_IrTypeId schema=type->collectionValueSchemaType;
            type->collectionValueSchemaType=ins->type; refuse(ir); type->collectionValueSchemaType=schema;
            XL6_IrDictKeyAbi key=type->dictKeyAbi; type->dictKeyAbi=XL6_IR_DICT_KEY_NONE; refuse(ir); type->dictKeyAbi=key;
            assert(i+1<ir->instructionCount && ir->instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
            XL6_IrInstruction* test=&ir->instructions[i+1]; size_t operands=test->operandCount;
            test->operandCount=0; refuse(ir); test->operandCount=operands;
            size_t count=0; XL6_IrTypeId chain[65];
            assert(xl6IrDictImportChainRecipe(ir,ins->type,chain,65,&count) && count && chain[0]==ins->type);
            size_t unchanged=99; XL6_IrTypeId entry=777;
            assert(!xl6IrDictImportChainRecipe(ir,ins->type,&entry,0,&unchanged)); assert(entry==777 && unchanged==99);
        }
        for (size_t i=0;i<ir->persistenceRestoreRootCount;++i) {
            XL6_IrPersistenceRestoreRoot* root=&ir->persistenceRestoreRoots[i]; assert(!root->pending);
            size_t count=root->seedCount; root->seedCount=count+1; refuse(ir); root->seedCount=count;
            for (size_t j=0;j<root->nodeCount;++j) {
                XL6_IrPersistenceRestoreNode* node=&ir->persistenceRestoreNodes[root->firstNode+j];
                if (node->abi!=XL6_IR_RESTORE_NODE_DICT) continue;
                ++restores; assert(node->carrierAbi==XL6_IR_COLLECTION_CARRIER_DICT_V1 && node->seed<root->seedCount);
                XL6_IrPersistenceRestoreNode saved=*node;
                node->carrierAbi=XL6_IR_COLLECTION_CARRIER_VALUE_V1; refuse(ir); *node=saved;
                node->abi=XL6_IR_RESTORE_NODE_LIST; refuse(ir); *node=saved;
                node->seed=SIZE_MAX; refuse(ir); *node=saved;
                node->element=SIZE_MAX; refuse(ir); *node=saved;
                node->identity^=1; refuse(ir); *node=saved;
            }
        }
        assert(xl6IrVerifyModule(ir,NULL)); xl6CompileUnitFree(&unit);
    }
    assert(imports>=3 && restores>=6 && mutations>=60);
    printf("Dict import source contracts: imports=%u restores=%u mutations=%u\n",imports,restores,mutations);
    return 0;
}
