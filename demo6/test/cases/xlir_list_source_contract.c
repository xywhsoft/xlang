#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static unsigned mutations;
static void refuse(const XL6_IrModule* ir)
{
    XL6_DiagList diags={0}; XL6_Output out; char* text;
    assert(!xl6IrVerifyModule(ir,&diags));
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitC(&out,ir,&diags));
    text=xl6OutputTakeMemoryText(&out,NULL); assert(!text || !*text); xrtFree(text);
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out,ir,"BAD_LIST_SOURCE",&diags));
    text=xl6OutputTakeMemoryText(&out,NULL); assert(!text || !*text); xrtFree(text);
    XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_FRAGMENT,&plan,&diags));
    assert(!plan.sourceCount && !plan.nativeModuleCount); xl6DiagFree(&diags); ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==4);
    unsigned creates=0,imports=0;
    for (unsigned lane=1;lane<4;++lane) {
        XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[lane],1));
        /* Erased List vocabulary exists even without an explicit source type;
         * presence of an IR type alone must not pull List into Array products. */
        assert(xl6IrListAnyType(&unit.ir)!=XL6_IR_TYPE_INVALID);
        for (size_t i=0;i<unit.ir.valueInitializerCount;++i) {
            XL6_IrValueInitializer* init=&unit.ir.valueInitializers[i];
            if (init->runtimeOp!=XL6_IR_RUNTIME_LIST_IMPORT_CHAIN) continue;
            assert(init->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
            init->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE; refuse(&unit.ir);
            init->errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE;
        }
        assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* ir=&unit.ir; XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE)==(lane!=3));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_IMPORT_CHAIN)==(lane==1));
        if (lane==2) {
            assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CHAR_VALUE));
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_IMPORT_CHAIN));
        }
        print_plan(lane==1 ? "ListSource" : lane==2 ? "ListFactory" : "ArrayWithoutList",&plan);
        for (size_t i=0;i<ir->instructionCount;++i) {
            XL6_IrInstruction* ins=&ir->instructions[i];
            bool importing=ins->runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN;
            if (!importing && ins->runtimeOp!=XL6_IR_RUNTIME_LIST_CREATE) continue;
            if (importing) ++imports; else ++creates;
            assert(importing ? xl6IrListImportRuntimeInstructionCompatible(ir,ins) :
                xl6IrListCreateRuntimeInstructionCompatible(ir,ins));
#define CHANGE(Member, Value) do { __typeof__(ins->Member) saved=ins->Member; \
    ins->Member=Value; refuse(ir); ins->Member=saved; } while(0)
            CHANGE(runtimeOp,importing ? XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND : XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW);
            CHANGE(runtimeOp,importing ? XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN : XL6_IR_RUNTIME_ARRAY_CREATE);
            CHANGE(operandCount,importing ? 0u : 1u);
            CHANGE(collectionAbi,XL6_IR_COLLECTION_ARRAY);
            CHANGE(payloadType,ins->type);
            CHANGE(runtimeIdentityType,ins->type);
            CHANGE(collectionAdmissionAbi,XL6_IR_COLLECTION_ADMISSION_PRIMITIVE);
#undef CHANGE
            XL6_IrType* type=&ir->types.items[ins->type-1];
            XL6_IrTypeId saved=type->listValueSchemaType;
            type->listValueSchemaType=ins->type; refuse(ir); type->listValueSchemaType=saved;
            size_t count=0; XL6_IrTypeId ids[65];
            assert(xl6IrListImportChainRecipe(ir,ins->type,ids,65,&count) && count && ids[0]==ins->type);
            size_t untouched=99; XL6_IrTypeId first=ids[0];
            assert(!xl6IrListImportChainRecipe(ir,ins->type,ids,count-1,&untouched) && untouched==99 && ids[0]==first);
            if (importing) {
                assert(i+1<ir->instructionCount && ir->instructions[i+1].kind==XL6_IR_INSTR_ERROR_TEST);
                XL6_IrInstruction* test=&ir->instructions[i+1];
                XL6_IrBlockId old=test->falseTarget;
                test->falseTarget=test->trueTarget; refuse(ir); test->falseTarget=old;
            }
        }
        assert(xl6IrVerifyModule(ir,&unit.parse.diags)); xl6CompileUnitFree(&unit);
    }
    assert(creates>=10 && imports>=10 && mutations>=100);
    printf("List source contracts: creates=%u imports=%u mutations=%u native producers and exact pruning verified\n",creates,imports,mutations);
    return 0;
}
