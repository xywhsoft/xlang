/* Reuse the actual typed/corrupt-IR, deterministic artifact contracts. */
#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
int main(int argc,char** argv)
{
    assert(argc==4);
    size_t rejected=0;
    for (int i=1;i<argc;++i) {
        assert(fixture_contract(argv[i]));
        XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[i],1));
        for (size_t n=0;n<unit.ir.intrinsicCallCount;++n) {
            XL6_IrIntrinsicCall* item=&unit.ir.intrinsicCalls[n];
            if (item->kind!=XL6_IR_INTRINSIC_COLLECTION_CONTAINS &&
                item->kind!=XL6_IR_INTRINSIC_COLLECTION_REMOVE &&
                !(item->kind==XL6_IR_INTRINSIC_TYPED_COLLECTION_MEMBER &&
                  item->typedCollectionOp==XL6_IR_TYPED_COLLECTION_TRIM_TO)) continue;
            assert(item->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE);
            XL6_DiagList diagnostic; xl6DiagInit(&diagnostic);
            item->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;
            assert(!xl6IrVerifyModule(&unit.ir,&diagnostic) && diagnostic.count);
            item->errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE; xl6DiagFree(&diagnostic); ++rejected;
        }
        assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_METHODS)==(i==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_LIST_NATIVE)==(i==1));
        if (i==1) {
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_CONTAINS));
            assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_COLLECTION_EXTRACT_VALUE));
        }
        xl6CompileUnitFree(&unit);
    }
    puts("List methods: actual List selection; Array/Dict exclude List; typed/corrupt IR refused; deterministic plans");
    assert(rejected>=7); printf("Collection error contracts: %zu NO_RAISE mutations rejected\n",rejected);
    return 0;
}
