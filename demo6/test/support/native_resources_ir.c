#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct Policy { const char* owner; const char* getter; bool target,field; } policies[]={
#define XL6_NATIVE_RESOURCE_POLICY(Owner,Component,Getter) {Owner,#Getter,false,false},
#include "src/runtime/xl6_native_resource_policies.inc"
#undef XL6_NATIVE_RESOURCE_POLICY
};
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
    assert(argc>2);size_t targets=0,fields=0;
    for(int input=1;input<argc-1;++input){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[input],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        for(size_t p=0;p<sizeof(policies)/sizeof(policies[0]);++p){
            struct Policy* policy=&policies[p];if(!policy->owner)continue;
            for(size_t i=0;i<unit.ir.callTargetCount;++i){
                XL6_IrCallTarget* target=&unit.ir.callTargets[i];
                if(!target->cName || strcmp(target->cName,policy->owner))continue;
                assert(target->nativeHandlePolicyAbi==XL6_IR_NATIVE_HANDLE_POLICY_RESIDENT_V1);
                assert(!strcmp(target->nativeHandlePolicyCName,policy->getter));
                if(policy->target)continue;
                policy->target=true;++targets;XL6_IrCallTarget saved=*target;
                target->nativeHandlePolicyAbi=XL6_IR_NATIVE_HANDLE_POLICY_NONE;target->nativeHandlePolicyCName=NULL;reject(&unit.ir);*target=saved;
                target->nativeHandlePolicyCName="xl6ProcessHandlePolicyV1Get";reject(&unit.ir);*target=saved;
            }
            for(size_t i=0;i<unit.ir.classFieldCount;++i){
                XL6_IrClassField* field=&unit.ir.classFields[i];
                if(!field->nativeHandlePolicyCName || strcmp(field->nativeHandlePolicyCName,policy->getter))continue;
                assert(field->nativeHandlePolicyAbi==XL6_IR_NATIVE_HANDLE_POLICY_RESIDENT_V1);
                if(policy->field)continue;
                policy->field=true;++fields;XL6_IrClassField saved=*field;
                field->nativeHandlePolicyAbi=XL6_IR_NATIVE_HANDLE_POLICY_NONE;field->nativeHandlePolicyCName=NULL;reject(&unit.ir);*field=saved;
                field->nativeHandlePolicyCName="xl6ProcessHandlePolicyV1Get";reject(&unit.ir);*field=saved;
            }
        }
        assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));xl6CompileUnitFree(&unit);
    }
    for(size_t p=0;p<sizeof(policies)/sizeof(policies[0]);++p){
        if(!policies[p].owner)continue;
        if(!policies[p].target || !policies[p].field)fprintf(stderr,"missing policy %s target=%d field=%d\n",policies[p].owner,policies[p].target,policies[p].field);
        assert(policies[p].target && policies[p].field);
    }
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[argc-1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_ArtifactRuntimePlan plan={0};assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
#define XL6_NATIVE_RESOURCE_POLICY(Owner,Component,Getter) assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_##Component));
#include "src/runtime/xl6_native_resource_policies.inc"
#undef XL6_NATIVE_RESOURCE_POLICY
    assert(plan.hostAbi==XL6_ARTIFACT_HOST_NONE);xl6CompileUnitFree(&unit);
    assert(targets==34 && fields==34 && rejected==136);
    printf("native policies targets=%zu fields=%zu rejected=%u; scalar provider pruning passed\n",targets,fields,rejected);return 0;
}
