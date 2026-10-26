/* Exact declarative contracts must fail closed before any C/header output. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned refusals;
static void refuse(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));refusals++;
    for(unsigned header=0;header<2;header++){
        XL6_Output out;XL6_DiagList diags;size_t size=SIZE_MAX;
        xl6DiagInit(&diags);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_RESUME",&diags):xl6IrCgenEmitC(&out,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&diags);
    }
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned profiles=0;
    for(size_t i=0;i<module->callTargetCount;i++){
        XL6_IrCallTarget* target=&module->callTargets[i];if(!target->cName)continue;
        bool array=!strcmp(target->cName,"xl6TlsResumeWithPeerCertificatesV1");
        if(!array&&strcmp(target->cName,"xl6TlsResumePeerCertificateCountV1")&&strcmp(target->cName,"xl6TlsResumePeerCertificateV1"))continue;
        profiles++;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));XL6_IrCallTarget saved=*target;
#define REFUSE(Field,Value) do{target->Field=(Value);refuse(module,target);*target=saved;}while(0)
        REFUSE(parameterCount,saved.parameterCount+1);
        REFUSE(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1);
        REFUSE(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        REFUSE(effectAbi,XL6_IR_CALL_EFFECT_CONSERVATIVE);
        if(array){assert(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1);
            REFUSE(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_NONE);}
#undef REFUSE
        assert(xl6IrVerifyModule(module,NULL));
    }
    assert(profiles==3&&refusals==13);
    xl6CompileUnitFree(&unit);
    puts("TLS resume peer XLIR: three exact profiles; 13 refusals and 26 empty-output gates");return 0;
}
