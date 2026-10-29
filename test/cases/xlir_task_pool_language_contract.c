#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t refusals;
static void reject(XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(target)assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_POOL",&errors):xl6IrCgenEmitC(&out,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&errors);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==4);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    assert(module->abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1);
    assert(module->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_NONE&&module->abi.taskPoolAbi==XL6_IR_MODULE_TASK_POOL_NONE);
    XL6_IrModuleAbi saved=module->abi;
    module->abi.executionOwnershipAbi=XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE;reject(module,NULL);module->abi=saved;
    unsigned factories=0,callbacks=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];XL6_IrCallTarget original=*target;
        if(target->cName&&!strcmp(target->cName,"xl6TaskPoolCreateLanguageV1")){
            assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
            target->nativeServicesRequired=0;reject(module,target);*target=original;
            target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;reject(module,target);*target=original;++factories;
        }
        if(!target->cName||strcmp(target->cName,"xl6TaskPoolSubmitLanguageV1"))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_NONE;reject(module,target);*target=original;
        target->nativeContextType=target->parameterTypes[0];reject(module,target);*target=original;
        target->nativeContextPayloadAbi=XL6_IR_ASYNC_RESULT_INVALID;reject(module,target);*target=original;
        target->nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1;reject(module,target);*target=original;
        target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;reject(module,target);*target=original;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;reject(module,target);*target=original;
        XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[1];
        target->parameterPhysicalAbis[1]=XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW;
        reject(module,target);target->parameterPhysicalAbis[1]=physical;
        XL6_IrParameterAbi parameter=target->parameterAbis[1];
        target->parameterAbis[1]=XL6_IR_PARAMETER_ABI_VALUE;reject(module,target);target->parameterAbis[1]=parameter;
        XL6_IrType* callback=(XL6_IrType*)xl6IrTypeGet(&module->types,target->parameterTypes[1]);XL6_IrType savedCallback=*callback;
        callback->paramCount=1;reject(module,target);*callback=savedCallback;
        callback->callableResultType=target->parameterTypes[0];reject(module,target);*callback=savedCallback;
        callback->nullAbi=XL6_IR_NULL_ABI_XVALUE_SENTINEL;reject(module,target);*callback=savedCallback;
        callback->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;reject(module,target);*callback=savedCallback;
        callback->cAbi=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;reject(module,target);*callback=savedCallback;
        callback->typeValueId=0;reject(module,target);*callback=savedCallback;
        ++callbacks;
    }
    assert(factories&&callbacks>=3&&xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    for(unsigned argument=2;argument<4;++argument){
        xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[argument],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        assert(unit.ir.abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE);
        unit.ir.abi.executionOwnershipAbi=XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1;reject(&unit.ir,NULL);
        unit.ir.abi.executionOwnershipAbi=XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE;assert(xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
    }
    printf("TaskPool XLIR: %zu pre-emission refusals, exact callback/result/profile/error/borrow/source contracts\n",refusals);
    return 0;
}
