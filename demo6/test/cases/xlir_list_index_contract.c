#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
int main(int argc,char** argv)
{
    assert(argc==5 && fixture_contract(argv[1]) && fixture_contract(argv[3]) && fixture_contract(argv[4]));
    XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
    const XL6_ArtifactRuntimeId required[]={XL6_ARTIFACT_VALUE_INDEX_READ,XL6_ARTIFACT_VALUE_INDEX_WRITE,
        XL6_ARTIFACT_LIST_VALUE_READ,XL6_ARTIFACT_LIST_NATIVE,XL6_ARTIFACT_VALUE_CONTAINS};
    for (size_t i=0;i<sizeof(required)/sizeof(required[0]);++i) assert(xl6ArtifactRuntimeSetHas(&plan.components,required[i]));
    XL6_Output out; assert(xl6OutputOpenFile(&out,argv[2])); unsigned bindings=0;
    for (size_t i=0;i<unit.ir.functionCount;++i) {
        const XL6_IrFunction* f=&unit.ir.functions[i]; if (!f->debugName) continue;
        const char* token=!strcmp(f->debugName,"indexRead") ? "READ" :
            !strcmp(f->debugName,"indexWrite") ? "WRITE" : !strcmp(f->debugName,"indexContains") ? "CONTAINS" : NULL;
        if (!token) continue;
        const XL6_IrType* type=xl6IrTypeGet(&unit.ir.types,f->returnType); assert(type && type->errorResultCName);
        xl6OutputPrintf(&out,"#define XL6_INDEX_%s xlir_fn_%u\n#define XL6_INDEX_%s_RESULT %s\n",
            token,(unsigned)f->symbol,token,type->errorResultCName); ++bindings;
    }
    for (size_t i=0;i<unit.ir.storageCount;++i) {
        const XL6_IrStorage* storage=&unit.ir.storages[i]; if (storage->kind!=XL6_IR_STORAGE_GLOBAL) continue;
        for (size_t j=0;j<unit.ir.nodeCount;++j) {
            const XL6_IrNode* node=&unit.ir.nodes[j];
            if (node->symbol==storage->symbol && node->text && !strcmp(node->text,"indexCleanups")) {
                xl6OutputPrintf(&out,"#define XL6_INDEX_CLEANUPS s%u\n",(unsigned)storage->symbol); ++bindings; break;
            }
        }
    }
    assert(bindings==4); xl6OutputClose(&out); xl6CompileUnitFree(&unit);
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[3],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
    for (size_t i=0;i<sizeof(required)/sizeof(required[0]);++i) assert(!xl6ArtifactRuntimeSetHas(&plan.components,required[i]));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_METHODS));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_VALUE_ITERATOR));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_METHODS_FIND));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_WIRE_CONTAINS));
    xl6CompileUnitFree(&unit);
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[4],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
    for (size_t i=0;i<sizeof(required)/sizeof(required[0]);++i) assert(!xl6ArtifactRuntimeSetHas(&plan.components,required[i]));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_TYPED));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_ARRAY_METHODS_FIND));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_WIRE_CONTAINS));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_METHODS));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_NATIVE));
    xl6CompileUnitFree(&unit);
    puts("Actual List var-index, Array and Set/Dict-only query plans verified"); return 0;
}
