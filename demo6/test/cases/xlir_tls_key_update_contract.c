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
        assert(xl6IrVerifyModule(&unit.ir,NULL)&&xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_WORKER)==(arg==1));
        if(arg==1){
            XL6_IrTypeId futureInt=0;
            for(size_t i=0;i<unit.ir.callTargetCount;++i){
                XL6_IrCallTarget* t=&unit.ir.callTargets[i];
                if(t->cName&&!strcmp(t->cName,"xl6TlsStreamSendBytesAsyncV1"))futureInt=t->returnType;
            }
            const XL6_IrType* future=xl6IrTypeGet(&unit.ir.types,futureInt);
            assert(future&&future->paramCount==1&&future->params);
            for(size_t i=0;i<unit.ir.callTargetCount;++i){
                XL6_IrCallTarget* target=&unit.ir.callTargets[i];
                bool asynchronous=target->cName&&!strcmp(target->cName,"xl6TlsStreamKeyUpdateAsyncV1");
                if(!asynchronous&&(!target->cName||strcmp(target->cName,"xl6TlsStreamKeyUpdateV1")))continue;
                ++targets;assert(xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));XL6_IrCallTarget saved=*target;
#define REFUSE(Field,Value) do{target->Field=(Value);assert(!xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));++refusals;*target=saved;}while(0)
                REFUSE(returnType,asynchronous?futureInt:future->params[0]);
                REFUSE(parameterCount,saved.parameterCount+1);
                REFUSE(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1);
                REFUSE(effectAbi,XL6_IR_CALL_EFFECT_PURE);
#undef REFUSE
            }
        }
        assert(xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
    }
    assert(targets==2&&refusals==8);
    puts("TLS KeyUpdate XLIR: two exact targets, 8 refusals, enum Future cannot be int, precise real plans: ok");return 0;
}
