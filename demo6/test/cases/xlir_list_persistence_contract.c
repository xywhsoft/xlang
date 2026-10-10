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
    assert(xl6OutputOpenMemory(&out) && !xl6IrCgenEmitHeader(&out,ir,"BAD_LIST_RESTORE",&diags));
    text=xl6OutputTakeMemoryText(&out,NULL); assert(!text || !*text); xrtFree(text);
    XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_FRAGMENT,&plan,&diags));
    assert(!plan.sourceCount && !plan.nativeModuleCount); xl6DiagFree(&diags); ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==5);
    for (unsigned lane=1;lane<3;++lane) {
        assert(fixture_contract(argv[lane]));
        XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[lane],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* ir=&unit.ir; XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_PERSISTENCE_PROJECT_LIST)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_GRAPH)==(lane==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CHAR_VALUE)==(lane==1));
        if (lane==1) {
            bool bound=false; unsigned lists=0;
            for (size_t r=0;r<ir->persistenceRestoreRootCount;++r) {
                XL6_IrPersistenceRestoreRoot* root=&ir->persistenceRestoreRoots[r];
                assert(!root->pending && root->seedCount);
                if (root->format==XL6_IR_PERSIST_FORMAT_JSON && !bound) {
                    const XL6_IrClassLayout* layout=NULL;
                    for(size_t i=0;i<ir->classLayoutCount;++i)
                        if(ir->classLayouts[i].type==root->type) { layout=&ir->classLayouts[i]; break; }
                    assert(layout && layout->persistenceDefaultFunction);
                    XL6_Output out; assert(xl6OutputOpenFile(&out,argv[3]));
                    xl6OutputPrintf(&out,"#define XL6_LIST_DEFAULT_FN xlir_fn_%u\n"
                        "#define XL6_LIST_DEFAULT_RESULT %s\n"
                        "#define XL6_LIST_PARSE_FN xlir_persist_json_parse_t%u\n"
                        "#define XL6_LIST_XLON_FN xlir_persist_xlon_parse_t%u\n"
                        "#define XL6_LIST_JSON_READ_FN xlir_persist_json_read_t%u\n"
                        "#define XL6_LIST_XLON_READ_FN xlir_persist_xlon_read_t%u\n",
                        (unsigned)ir->functions[layout->persistenceDefaultFunction-1].symbol,
                        xl6IrTypeGet(&ir->types,root->type)->errorResultCName,
                        (unsigned)root->type,(unsigned)root->type,(unsigned)root->type,(unsigned)root->type);
                    xl6OutputClose(&out); bound=true;
                }
                uint32 saved=root->pending; root->pending=XL6_IR_RESTORE_PENDING_LIST; refuse(ir); root->pending=saved;
                size_t count=root->seedCount; root->seedCount=count+1; refuse(ir); root->seedCount=count;
            }
            for (size_t n=0;n<ir->persistenceRestoreNodeCount;++n) {
                XL6_IrPersistenceRestoreNode* node=&ir->persistenceRestoreNodes[n];
                if (node->abi!=XL6_IR_RESTORE_NODE_LIST) continue;
                ++lists; assert(node->carrierAbi==XL6_IR_COLLECTION_CARRIER_LIST_V1 && node->seed!=SIZE_MAX);
#define CHANGE(Member, Value) do { __typeof__(node->Member) saved=node->Member; \
    node->Member=Value; refuse(ir); node->Member=saved; } while(0)
                CHANGE(carrierAbi,XL6_IR_COLLECTION_CARRIER_VALUE_V1);
                CHANGE(seed,SIZE_MAX); CHANGE(element,SIZE_MAX); CHANGE(identity,0);
#undef CHANGE
                XL6_IrType* type=&ir->types.items[node->payloadType-1];
                XL6_IrCollectionCarrierAbi saved=type->persistenceRestoreCarrierAbi;
                type->persistenceRestoreCarrierAbi=XL6_IR_COLLECTION_CARRIER_VALUE_V1;
                refuse(ir); type->persistenceRestoreCarrierAbi=saved;
            }
            assert(bound && lists>=8); print_plan("List",&plan);
        }
        assert(xl6IrVerifyModule(ir,&unit.parse.diags)); xl6CompileUnitFree(&unit);
    }
    assert(fixture_contract(argv[4]));
    XL6_CompileUnit only; xl6CompileUnitInit(&only);
    assert(xl6CompileUnitLoadFile(&only,argv[4],1) && xl6IrLowerModule(&only.ir,&only.parse.diags));
    XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(&only.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&only.parse.diags));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_IMPORT_CHAIN));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_ERASED));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_PERSISTENCE_PROJECT_LIST));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_GRAPH));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_CHAR_VALUE));
    print_plan("ListRestoreOnly",&plan); xl6CompileUnitFree(&only);
    printf("List persistence: %u corrupt plans refused; native restore/projection and leaf pruning verified\n",mutations);
    return 0;
}
