#include "src/compile/xl6_compile_unit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool selected(const char* name)
{
    return name&&(!strncmp(name,"xl6TlsServerRequest",19)||!strncmp(name,"xl6TlsServerChoice",18)||
        !strcmp(name,"xl6TlsServerContextSelectorCreateV1")||!strcmp(name,"xl6TlsServerContextWithSelectorV1")||
        !strcmp(name,"xl6TlsServerContextHasSelectorV1"));
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    size_t targets=0,callbacks=0,refusals=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];if(!selected(target->cName))continue;
        targets++;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));XL6_IrCallTarget saved=*target;
#define REFUSE(Field,Value) do{target->Field=(Value);assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));refusals++;*target=saved;}while(0)
        REFUSE(parameterCount,saved.parameterCount+1);
        REFUSE(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1);
        REFUSE(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_CALLBACK_V1){
            XL6_IrNativeClassCallbackPlan plan={0};assert(xl6IrNativeClassCallbackPlanBuild(module,target,&plan));callbacks++;
            assert(plan.signature==saved.nativeContextType&&plan.argument&&plan.result&&plan.argument!=plan.result);
            assert(!strcmp(plan.argumentField->nativeHandlePolicyCName,"xl6TlsServerRequestHandlePolicyV1Get"));
            assert(!strcmp(plan.resultField->nativeHandlePolicyCName,"xl6TlsServerChoiceHandlePolicyV1Get"));
            REFUSE(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_NONE);
            REFUSE(nativeContextType,plan.argument);
            XL6_IrType* f=&module->types.items[plan.signature-1];
            XL6_IrParameterMode mode=f->parameterModes[0];f->parameterModes[0]=XL6_IR_PARAMETER_MODE_BYREF;
            assert(!xl6IrNativeClassCallbackPlanBuild(module,target,NULL));refusals++;f->parameterModes[0]=mode;
            XL6_IrTypeId input=f->params[0];f->params[0]=plan.result;
            assert(!xl6IrNativeClassCallbackPlanBuild(module,target,NULL));refusals++;f->params[0]=input;
            XL6_IrType* nullable=&module->types.items[f->returns[0]-1];XL6_IrTypeId result=nullable->params[0];
            nullable->params[0]=plan.argument;assert(!xl6IrNativeClassCallbackPlanBuild(module,target,NULL));refusals++;nullable->params[0]=result;
            size_t count=f->paramCount;f->paramCount=0;
            assert(!xl6IrNativeClassCallbackPlanBuild(module,target,NULL));refusals++;f->paramCount=count;
            assert(xl6IrNativeClassCallbackPlanBuild(module,target,NULL));
        }
#undef REFUSE
    }
    assert(targets==14&&callbacks==2&&refusals>=50&&xl6IrVerifyModule(module,NULL));
    printf("TLS selector XLIR: %zu targets; %zu exact class callbacks; %zu refusals\n",targets,callbacks,refusals);
    xl6CompileUnitFree(&unit);return 0;
}
