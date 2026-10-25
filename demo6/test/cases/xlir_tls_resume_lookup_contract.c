#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool selected(const char* n)
{
    return n&&(!strncmp(n,"xl6TlsResumeRequest",sizeof("xl6TlsResumeRequest")-1)||
        !strcmp(n,"xl6TlsServerContextWithResumeLookupV1")||!strcmp(n,"xl6TlsServerContextWithoutResumeLookupV1")||
        !strcmp(n,"xl6TlsServerContextHasResumeLookupV1")||!strcmp(n,"xl6TlsServerContextResumeAgeToleranceV1"));
}
int main(int argc,char** argv)
{
    assert(argc==5);size_t targets=0,callbacks=0,refusals=0;
    for(int arg=1;arg<argc;++arg){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);XL6_ArtifactRuntimePlan plan;
        assert(xl6CompileUnitLoadFile(&unit,argv[arg],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
        assert(xl6ArtifactRuntimePlanBuild(module,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
        bool dynamic=arg<=2;
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_RESUME_LOOKUP)==dynamic);
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_RESUME_REQUEST)==dynamic);
        if(arg==4)assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_RESUME));
        if(arg==1)for(size_t i=0;i<module->callTargetCount;++i){
            XL6_IrCallTarget* target=&module->callTargets[i];if(!selected(target->cName))continue;
            ++targets;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));XL6_IrCallTarget saved=*target;
#define REFUSE(Field,Value) do{target->Field=(Value);assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));++refusals;*target=saved;}while(0)
            REFUSE(parameterCount,saved.parameterCount+1);
            REFUSE(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1);
            REFUSE(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
            if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_CALLBACK_V1){
                XL6_IrNativeClassCallbackPlan recipe={0};assert(xl6IrNativeClassCallbackPlanBuild(module,target,&recipe));++callbacks;
                assert(!strcmp(recipe.argumentField->nativeHandlePolicyCName,"xl6TlsResumeRequestHandlePolicyV1Get"));
                assert(!strcmp(recipe.resultField->nativeHandlePolicyCName,"xl6TlsResumeHandlePolicyV1Get"));
                REFUSE(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_NONE);REFUSE(nativeContextType,recipe.argument);
            }
#undef REFUSE
        }
        assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    }
    assert(targets==10&&callbacks==1&&refusals==32);
    puts("TLS resume lookup XLIR: 10 exact targets, typed callback, 32 refusals, four real pruning plans: ok");return 0;
}
