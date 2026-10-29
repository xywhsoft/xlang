#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool selected(const char* n)
{return n&&(!strcmp(n,"xl6TlsClientContextWithResumeV1")||!strcmp(n,"xl6TlsClientContextWithoutResumeV1")||
    !strcmp(n,"xl6TlsClientContextHasResumeV1")||!strcmp(n,"xl6TlsClientContextResumeV1")||!strcmp(n,"xl6TlsSessionResumedV1"));}
int main(int argc,char** argv)
{
    assert(argc==5);size_t targets=0,refusals=0;
    for(int arg=1;arg<argc;++arg){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);XL6_ArtifactRuntimePlan plan;
        assert(xl6CompileUnitLoadFile(&unit,argv[arg],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        assert(xl6IrVerifyModule(&unit.ir,NULL)&&xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
        bool bound=arg<=2;
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_CLIENT_CONTEXT_RESUME)==bound);
        bool overlay=false;for(size_t i=0;i<plan.nativeModuleCount;++i)overlay|=!strcmp(plan.nativeModules[i],"XRT_MODULE_TLS_CLIENT_RESUME");
        assert(overlay==bound);
        if(arg==1)for(size_t i=0;i<unit.ir.callTargetCount;++i){
            XL6_IrCallTarget* target=&unit.ir.callTargets[i];if(!selected(target->cName))continue;
            ++targets;assert(xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));XL6_IrCallTarget saved=*target;
#define REFUSE(Field,Value) do{target->Field=(Value);assert(!xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));++refusals;*target=saved;}while(0)
            REFUSE(parameterCount,saved.parameterCount+1);
            REFUSE(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1);
            REFUSE(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
#undef REFUSE
        }
        assert(xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
    }
    assert(targets==5&&refusals==15);
    puts("TLS client resume XLIR: 5 exact targets, 15 refusals, four real plans and no unused client-resume overlay: ok");return 0;
}
