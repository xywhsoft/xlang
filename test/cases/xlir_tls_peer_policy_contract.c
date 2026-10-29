#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned refusals;
static void refuse(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    assert(!xl6IrNativeClassCallbackPlanBuild(module,target,NULL)||
        !xl6IrRuntimeDirectBridgeTargetMatches(module,target));refusals++;
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;header++){
        XL6_Output out;XL6_DiagList diags;size_t size=SIZE_MAX;
        xl6DiagInit(&diags);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_PREDICATE",&diags):xl6IrCgenEmitC(&out,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&diags);
    }
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));XL6_IrTypeId integer=0;
    for(size_t i=0;i<module->types.count;i++)if(module->types.items[i].kind==XL6_IR_TYPE_BUILTIN&&
        module->types.items[i].cAbi==XL6_IR_C_ABI_I64)integer=(XL6_IrTypeId)i+1;
    assert(integer);unsigned predicates=0;
    for(size_t i=0;i<module->callTargetCount;i++){
        XL6_IrCallTarget* target=&module->callTargets[i];
        if(!target->cName||strcmp(target->cName,"xl6TlsVerifierCreateCallbackV1"))continue;
        predicates++;XL6_IrCallTarget saved=*target;XL6_IrNativeClassCallbackPlan plan={0};
        assert(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_PREDICATE_V1&&
            xl6IrNativeClassCallbackPlanBuild(module,target,&plan)&&plan.argumentField&&!plan.resultField);
        XL6_IrType* function=&module->types.items[plan.signature-1];
        XL6_IrTypeId result=function->returns[0];
        function->returns[0]=function->callableResultType=integer;refuse(module,target);
        function->returns[0]=function->callableResultType=result;
        XL6_IrTypeId argument=function->params[0];function->params[0]=integer;refuse(module,target);function->params[0]=argument;
        size_t count=function->paramCount;function->paramCount=0;refuse(module,target);function->paramCount=count;
        XL6_IrParameterAbi abi=function->parameterAbis[0];function->parameterAbis[0]=XL6_IR_PARAMETER_ABI_VALUE;
        refuse(module,target);function->parameterAbis[0]=abi;
        target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_CALLBACK_V1;refuse(module,target);*target=saved;
        XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[2];
        target->parameterPhysicalAbis[2]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refuse(module,target);
        target->parameterPhysicalAbis[2]=physical;*target=saved;
        target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;refuse(module,target);*target=saved;
        assert(xl6IrVerifyModule(module,NULL));
    }
    assert(predicates==1&&refusals==7);
    printf("TLS peer predicate XLIR: exact bool/class/arity/borrow/context/physical/error; 7 refusals and 14 empty-output gates\n");
    xl6CompileUnitFree(&unit);return 0;
}
