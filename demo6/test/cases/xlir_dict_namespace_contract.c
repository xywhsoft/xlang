#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(!xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,ir,"BAD_DICT_NAMESPACE",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(!bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));XL6_IrModule* ir=&unit.ir;
    assert(xl6IrLowerModule(ir,&unit.parse.diags) && xl6IrVerifyModule(ir,NULL));unsigned calls=0;
    for(size_t i=0;i<ir->callTargetCount;++i) {
        XL6_IrCallTarget* target=&ir->callTargets[i];
        if(!target->cName || strcmp(target->cName,"xl6DictValueRemoveV1"))continue;
        assert(target->runtimeBridge==XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && target->parameterCount==2);
        assert(target->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE && xl6IrRuntimeDirectBridgeTargetMatches(ir,target));
        XL6_IrCallTarget saved=*target;
        target->parameterCount=3;refused(ir);*target=saved;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(ir);*target=saved;
        target->runtimeBridge=XL6_IR_RUNTIME_BRIDGE_RESERVED_TABLE_REMOVE;refused(ir);*target=saved;
        XL6_IrParameterAbi abi=target->parameterAbis[0];assert(abi==XL6_IR_PARAMETER_ABI_BORROW);
        target->parameterAbis[0]=XL6_IR_PARAMETER_ABI_VALUE;
        refused(ir);target->parameterAbis[0]=abi;
        ++calls;
    }
    assert(calls>0 && xl6IrVerifyModule(ir,NULL));XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_DICT_METHODS));
    printf("Dict namespace direct contracts=%u mutations=%u\n",calls,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
