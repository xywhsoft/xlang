#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    static const char* names[]={"xl6TlsTicketResultStatusV1","xl6TlsTicketResultResumeV1",
        "xl6TlsSessionTicketCountV1","xl6TlsSessionDroppedTicketsV1","xl6TlsSessionTryTakeTicketV1",
        "xl6TlsSessionIssueNewTicketV1","xl6TlsSessionIssueTicketV1","xl6TlsClientContextWithRequiredResumeV1",
        "xl6TlsClientContextWithTicketCapacityV1","xl6TlsClientContextTicketCapacityV1","xl6TlsClientContextResumeRequiredV1"};
    assert(argc==4);unsigned seen=0;size_t refusals=0,slots=0;
    for(int arg=1;arg<argc;++arg){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);XL6_ArtifactRuntimePlan plan;
        assert(xl6CompileUnitLoadFile(&unit,argv[arg],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        assert(xl6IrVerifyModule(&unit.ir,NULL)&&xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
        bool producer=arg==1;
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_SESSION_TICKET_CLIENT)==producer);
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_SESSION_TICKET_SERVER)==producer);
        for(size_t i=0;i<unit.ir.callTargetCount&&producer;++i){
            XL6_IrCallTarget* target=&unit.ir.callTargets[i];size_t n=0;
            while(n<sizeof(names)/sizeof(*names)&&(!target->cName||strcmp(target->cName,names[n])))++n;
            if(n==sizeof(names)/sizeof(*names))continue;
            seen|=1u<<n;assert(xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));XL6_IrCallTarget saved=*target;
#define REFUSE(Field,Value) do{target->Field=(Value);assert(!xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));++refusals;*target=saved;}while(0)
            REFUSE(parameterCount,saved.parameterCount+1);
            REFUSE(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1);
            REFUSE(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
            if(saved.nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1){
                ++slots;assert(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1);
                assert(saved.nativeContextType==saved.returnType&&xl6IrNativeClassResultContextField(&unit.ir,target));
                REFUSE(nativeResultAbi,XL6_IR_CALL_NATIVE_RESULT_NONE);
                REFUSE(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_NONE);
                REFUSE(returnType,saved.parameterTypes[0]);
            }
#undef REFUSE
        }
        assert(xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
    }
    assert(seen==((1u<<11)-1)&&slots==3&&refusals==42);
    puts("TLS tickets XLIR: 11 exact targets, three structural class output slots, 42 refusals, real pruning plans: ok");return 0;
}
