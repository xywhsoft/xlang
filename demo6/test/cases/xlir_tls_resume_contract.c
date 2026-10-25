#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char** argv)
{
    assert(argc==3);size_t targets=0,refusals=0;
    for(int arg=1;arg<argc;++arg){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);XL6_ArtifactRuntimePlan plan;
        assert(xl6CompileUnitLoadFile(&unit,argv[arg],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
        assert(xl6ArtifactRuntimePlanBuild(module,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_RESUME)==(arg==1));
        /* The immutable credential does not require a TLS endpoint/handshake. */
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_SESSION_CLIENT));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_SESSION_SERVER));
        assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_SELECTOR));
        for(size_t i=0;i<module->callTargetCount;++i){
            XL6_IrCallTarget* target=&module->callTargets[i];
            if(!target->cName||strncmp(target->cName,"xl6TlsResume",12))continue;
            ++targets;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));XL6_IrCallTarget saved=*target;
#define REFUSE(Field,Value) do{target->Field=(Value);assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));++refusals;*target=saved;}while(0)
            REFUSE(parameterCount,saved.parameterCount+1);
            REFUSE(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1);
            REFUSE(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
#undef REFUSE
        }
        assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    }
    assert(targets==16&&refusals==48);
    puts("TLS resume XLIR: 16 sealed targets, 48 refusals, real credential/empty pruning: ok");return 0;
}
