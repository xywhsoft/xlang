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
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_SET_SOURCE",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(size==0); xrtFree(text); xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==4); unsigned creates=0,imports=0,checkedAlgebra=0;
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_SET_IMPORT_CHAIN,0)==XL6_IR_OPERAND_OWNERSHIP_CONSUME);
    assert(xl6IrRuntimeOperandOwnership(XL6_IR_RUNTIME_SET_IMPORT_CHAIN,1)==XL6_IR_OPERAND_OWNERSHIP_OBSERVE);
    for (unsigned lane=1;lane<4;++lane) {
        assert(fixture_contract(argv[lane])); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[lane],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* ir=&unit.ir; XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_NATIVE)==(lane!=3));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_METHODS)==(lane!=3));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_SET_IMPORT_CHAIN)==(lane!=3));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_RESTORE_CLASS_SET)==(lane==2));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_RESTORE_CLASS_COLLECTIONS)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_GRAPH)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_PERSISTENCE_PROJECT_COLLECTIONS)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_COPY_SET_GRAPH)==(lane!=3));
        if (lane==1) {
            assert(ir->persistenceRestoreRootCount);
            unsigned closed=0,legacy=0;
            for (size_t i=0;i<ir->persistenceRestoreRootCount;++i)
                if (ir->persistenceRestoreRoots[i].pending) ++legacy; else ++closed;
            /* Dict restoration still uses the legacy plan, but any nested Set
             * must leave it as a genuine native owner, not a wire carrier. */
            assert(closed && legacy);
        }
        for (size_t i=0;i<ir->instructionCount;++i) {
            XL6_IrInstruction* ins=&ir->instructions[i];
            switch (ins->runtimeOp) {
            case XL6_IR_RUNTIME_XVALUE_SET_UNION:
            case XL6_IR_RUNTIME_XVALUE_SET_INTERSECTION:
            case XL6_IR_RUNTIME_XVALUE_SET_DIFFERENCE:
            case XL6_IR_RUNTIME_XVALUE_SET_SYMMETRIC_DIFFERENCE:
            case XL6_IR_RUNTIME_XVALUE_SET_SUBSET:
            case XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUBSET:
            case XL6_IR_RUNTIME_XVALUE_SET_SUPERSET:
            case XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUPERSET: {
                assert(i+1<ir->instructionCount);
                XL6_IrInstruction* next=&ir->instructions[i+1];
                assert(next->kind==XL6_IR_INSTR_ERROR_TEST && next->operandCount==1);
                assert(xl6IrCallInstructionErrorPhysicalAbi(ir,ins)!=XL6_IR_CALL_ERROR_PHYSICAL_INVALID);
                size_t old=next->operandCount; next->operandCount=0; refuse(ir); next->operandCount=old;
                ++checkedAlgebra; break;
            }
            default: break;
            }
            bool importing=ins->runtimeOp==XL6_IR_RUNTIME_SET_IMPORT_CHAIN;
            if (!importing && ins->runtimeOp!=XL6_IR_RUNTIME_XVALUE_SET_NEW) continue;
            if (importing) ++imports; else ++creates;
            assert(importing ? xl6IrSetImportRuntimeInstructionCompatible(ir,ins) : xl6IrSetCreateRuntimeInstructionCompatible(ir,ins));
#define CHANGE(Member,Value) do { __typeof__(ins->Member) old=ins->Member; ins->Member=Value; refuse(ir); ins->Member=old; } while(0)
            CHANGE(operandCount,importing ? 0u : 1u);
            CHANGE(collectionAbi,XL6_IR_COLLECTION_ARRAY);
            CHANGE(collectionAbi,XL6_IR_COLLECTION_NONE);
            CHANGE(payloadType,ins->type);
            CHANGE(runtimeIdentityType,ins->type);
            CHANGE(collectionAdmissionAbi,XL6_IR_COLLECTION_ADMISSION_PRIMITIVE);
            CHANGE(runtimeOp,importing ? XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN : XL6_IR_RUNTIME_ARRAY_CREATE);
#undef CHANGE
            XL6_IrType* type=&ir->types.items[ins->type-1];
            XL6_IrTypeId saved=type->collectionValueSchemaType;
            type->collectionValueSchemaType=ins->type; refuse(ir); type->collectionValueSchemaType=saved;
            size_t count=0; XL6_IrTypeId chain[65];
            assert(xl6IrSetImportChainRecipe(ir,ins->type,chain,65,&count) && count && chain[0]==ins->type);
            size_t untouched=99; XL6_IrTypeId entry=777;
            assert(!xl6IrSetImportChainRecipe(ir,ins->type,&entry,0,&untouched)); assert(untouched==99 && entry==777);
        }
        assert(xl6IrVerifyModule(ir,&unit.parse.diags)); xl6CompileUnitFree(&unit);
    }
    assert(creates>=8 && imports>=1 && mutations>=72 && checkedAlgebra>=12);
    printf("Set source contracts: creates=%u imports=%u mutations=%u native factories and pruning verified\n",creates,imports,mutations);
    return 0;
}
