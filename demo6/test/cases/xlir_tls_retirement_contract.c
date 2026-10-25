#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char** argv)
{
    assert(argc > 1);
    size_t direct=0,mutations=0;
    for(int arg=1;arg<argc;++arg){
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[arg],1));
        assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        assert(xl6IrVerifyModule(&unit.ir,NULL));
        XL6_ArtifactRuntimePlan plan={0};
        assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,&unit.parse.diags));
        for(size_t i=0;i<unit.ir.callTargetCount;++i){
            XL6_IrCallTarget* target=&unit.ir.callTargets[i];
            if(!target->cName)continue;
            assert(strncmp(target->cName,"xl6_xrt_tls_",12));
            if(strncmp(target->cName,"xl6Tls",6))continue;
            ++direct;assert(xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));
            XL6_IrCallTarget saved=*target;
            target->parameterCount=saved.parameterCount+1;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));++mutations;*target=saved;
            target->effectAbi=saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?
                XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));++mutations;*target=saved;
        }
        assert(xl6IrVerifyModule(&unit.ir,NULL));
        xl6CompileUnitFree(&unit);
    }
    assert(direct>=20 && mutations==direct*2);
    printf("TLS retirement: %zu sealed resident targets, %zu refused mutations; no old TLS bridge: ok\n",direct,mutations);
    return 0;
}
