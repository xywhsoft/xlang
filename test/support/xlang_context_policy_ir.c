#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned rejected;
static void reject(XL6_IrModule* module)
{
    XL6_DiagList diagnostics={0};XL6_Output output;size_t length;
    assert(!xl6IrVerifyModule(module,&diagnostics) && diagnostics.count);
    assert(xl6OutputOpenMemory(&output));assert(!xl6IrCgenEmitC(&output,module,&diagnostics));
    char* text=xl6OutputTakeMemoryText(&output,&length);assert(!length);
    xrtFree(text);xl6DiagFree(&diagnostics);++rejected;
}
int main(int argc,char** argv)
{
    assert(argc==3);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    size_t targets=0,fields=0;
    for(size_t i=0;i<unit.ir.callTargetCount;++i){
        XL6_IrCallTarget* target=&unit.ir.callTargets[i];
        if(!target->cName || strcmp(target->cName,"xl6_xlang_context_destroy"))continue;
        assert(target->nativeHandlePolicyAbi==XL6_IR_NATIVE_HANDLE_POLICY_RESIDENT_V1 &&
            !strcmp(target->nativeHandlePolicyCName,"xl6XlangContextHandlePolicyV1Get"));++targets;
        XL6_IrCallTarget saved=*target;
        target->nativeHandlePolicyAbi=XL6_IR_NATIVE_HANDLE_POLICY_NONE;target->nativeHandlePolicyCName=NULL;reject(&unit.ir);*target=saved;
        target->nativeHandlePolicyCName="xl6ProcessHandlePolicyV1Get";reject(&unit.ir);*target=saved;
    }
    for(size_t i=0;i<unit.ir.classFieldCount;++i){
        XL6_IrClassField* field=&unit.ir.classFields[i];
        if(!field->nativeHandlePolicyCName || strcmp(field->nativeHandlePolicyCName,"xl6XlangContextHandlePolicyV1Get"))continue;
        ++fields;XL6_IrClassField saved=*field;
        field->nativeHandlePolicyAbi=XL6_IR_NATIVE_HANDLE_POLICY_NONE;field->nativeHandlePolicyCName=NULL;reject(&unit.ir);*field=saved;
        field->nativeHandlePolicyCName="xl6ProcessHandlePolicyV1Get";reject(&unit.ir);*field=saved;
    }
    assert(targets && fields && rejected);XL6_ArtifactRuntimePlan plan={0};
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_XLANG_CONTEXT_POLICY));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_XLANG_CONTEXT));
    assert(plan.hostAbi==XL6_ARTIFACT_HOST_XLANG_RUNTIME_V1);
    XL6_ArtifactOwnershipPlan ownership={0};assert(xl6ArtifactOwnershipPlanBuild(&plan,&ownership));
    plan.hostAbi=0;assert(!xl6ArtifactOwnershipPlanBuild(&plan,&ownership));
    XL6_Output output;size_t length;assert(xl6OutputOpenMemory(&output));
    assert(xl6IrCgenEmitC(&output,&unit.ir,&unit.parse.diags));char* text=xl6OutputTakeMemoryText(&output,&length);
    assert(length && strstr(text,"xl6NativeHandleTakeV1(handle, xl6XlangContextHandlePolicyV1Get())"));
    assert(!strstr(text,"xlir_native_handle_ops_"));xrtFree(text);xl6CompileUnitFree(&unit);
    xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[2],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_XLANG_CONTEXT_POLICY));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_OWNERSHIP_DOMAIN_XLANG_CONTEXT));
    assert(plan.hostAbi==XL6_ARTIFACT_HOST_NONE);
    xl6CompileUnitFree(&unit);
    printf("context policy targets=%zu fields=%zu rejected=%u; direct plan pruned\n",targets,fields,rejected);return 0;
}
