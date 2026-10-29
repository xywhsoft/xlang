#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    static const char* names[]={"xl6TlsStreamTicketCountV1","xl6TlsStreamDroppedTicketsV1",
        "xl6TlsStreamTicketCountAsyncV1","xl6TlsStreamDroppedTicketsAsyncV1","xl6TlsStreamTryTakeTicketV1",
        "xl6TlsStreamTryTakeTicketAsyncV1","xl6TlsStreamIssueNewTicketV1","xl6TlsStreamIssueTicketV1",
        "xl6TlsStreamIssueNewTicketAsyncV1","xl6TlsStreamIssueTicketAsyncV1"};
    assert(argc==6);unsigned seen=0;size_t refusals=0,recipes=0;
    for(int arg=1;arg<argc;++arg){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);XL6_ArtifactRuntimePlan plan;
        assert(xl6CompileUnitLoadFile(&unit,argv[arg],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        assert(xl6IrVerifyModule(&unit.ir,NULL)&&xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
        bool client=arg==1||arg==2,server=arg==1||arg==3,stats=arg==1||arg==4;
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_TICKET_CLIENT)==client);
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_TICKET_SERVER)==server);
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_TICKET_STATS)==stats);
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_TICKET_WAIT)==(arg==1));
        assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_TLS_TICKET_RESULT_PREPARE)==(client||server));
        for(size_t i=0;i<unit.ir.callTargetCount&&arg==1;++i){
            XL6_IrCallTarget* target=&unit.ir.callTargets[i];size_t n=0;
            while(n<10&&(!target->cName||strcmp(target->cName,names[n])))++n;
            if(n==10)continue;
            seen|=1u<<n;assert(xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));XL6_IrCallTarget saved=*target;
#define REFUSE(Field,Value) do{target->Field=(Value);assert(!xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));++refusals;*target=saved;}while(0)
            REFUSE(parameterCount,saved.parameterCount+1);
            REFUSE(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1);
            REFUSE(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
            if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1){
                ++recipes;assert(xl6IrNativeClassResultContextField(&unit.ir,target));
                REFUSE(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_NONE);
                REFUSE(nativeContextType,saved.parameterTypes[0]);
                REFUSE(nativeResultAbi,saved.nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_NONE?
                    XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1:XL6_IR_CALL_NATIVE_RESULT_NONE);
            }
#undef REFUSE
        }
        assert(xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
    }
    assert(seen==1023&&recipes==6&&refusals==48);
    puts("TLS transport ticket XLIR: 10 exact targets, six class recipes, 48 refusals, split client/server/stats/wait pruning: ok");return 0;
}
