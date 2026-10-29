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
    for(unsigned header=0;header<2;++header){XL6_Output out;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_EXECUTOR",&errors):xl6IrCgenEmitC(&out,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&errors);}
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==3||argc==4);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    assert(module->abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1);
    assert(module->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_NONE&&module->abi.taskPoolAbi==XL6_IR_MODULE_TASK_POOL_NONE);
    XL6_IrModuleAbi saved=module->abi;
    module->abi.executionOwnershipAbi=XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE;reject(module,NULL);module->abi=saved;
    module->abi.executionOwnershipAbi=(XL6_IrModuleExecutionOwnershipAbi)99;reject(module,NULL);module->abi=saved;
    module->abi.continuationAbi=XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1;reject(module,NULL);module->abi=saved;
    module->abi.retirementAbi=XL6_IR_MODULE_RETIREMENT_PREPARED_GRAPH_V2;reject(module,NULL);module->abi=saved;
    unsigned constructors=0,callbacks=0;
    for(size_t i=0;i<module->callTargetCount;++i){XL6_IrCallTarget* target=&module->callTargets[i];XL6_IrCallTarget copy=*target;
        if(target->cName&&!strcmp(target->cName,"xl6ExecutorCreateLanguageV1")){
            assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
            const unsigned bad[]={0,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1,~0u};
            for(size_t j=0;j<sizeof(bad)/sizeof(*bad);++j){target->nativeServicesRequired=bad[j];reject(module,target);*target=copy;}
            target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;reject(module,target);*target=copy;
            target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;reject(module,target);*target=copy;++constructors;
        }
        if(target->cName&&!strcmp(target->cName,"xl6ExecutorSubmitLanguageV1")){
            assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
            XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[1];
            target->parameterPhysicalAbis[1]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;reject(module,target);target->parameterPhysicalAbis[1]=physical;
            XL6_IrType* callback=(XL6_IrType*)xl6IrTypeGet(&module->types,target->parameterTypes[1]);XL6_IrType original=*callback;
            callback->paramCount=1;reject(module,target);*callback=original;
            callback->returnCount=1;reject(module,target);*callback=original;
            callback->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;reject(module,target);*callback=original;++callbacks;
        }
    }
    assert(constructors&&callbacks&&xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[2],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(unit.ir.abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE);
    unit.ir.abi.executionOwnershipAbi=XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1;reject(&unit.ir,NULL);
    unit.ir.abi.executionOwnershipAbi=XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE;assert(xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
    printf("Executor XLIR: %zu pre-emission refusals, exact source/closure/error/ownership contracts and unused-source pruning\n",refusals);
    if(argc==4){
        size_t before=refusals;unsigned batches=0;xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[3],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));module=&unit.ir;
        assert(xl6IrVerifyModule(module,NULL));
        for(size_t i=0;i<module->callTargetCount;++i){XL6_IrCallTarget* target=&module->callTargets[i];
            if(!target->cName||strcmp(target->cName,"xl6ExecutorSubmitBatchLanguageV1"))continue;
            XL6_IrCallTarget original=*target;
            target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_NONE;reject(module,target);*target=original;
            target->nativeContextType=target->parameterTypes[0];reject(module,target);*target=original;
            target->nativeContextPayloadAbi=XL6_IR_ASYNC_RESULT_RETAINED_XVALUE;reject(module,target);*target=original;
            target->nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1;reject(module,target);*target=original;
            target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;reject(module,target);*target=original;
            target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;reject(module,target);*target=original;
            XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[1];
            target->parameterPhysicalAbis[1]=XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW;reject(module,target);
            target->parameterPhysicalAbis[1]=physical;
            XL6_IrParameterAbi parameter=target->parameterAbis[1];
            target->parameterAbis[1]=XL6_IR_PARAMETER_ABI_VALUE;reject(module,target);target->parameterAbis[1]=parameter;
            XL6_IrType* array=(XL6_IrType*)xl6IrTypeGet(&module->types,target->parameterTypes[1]);XL6_IrType savedArray=*array;
            array->sequenceLayout.abi=XL6_IR_SEQUENCE_LAYOUT_REFERENCE_V2;reject(module,target);*array=savedArray;
            array->sequenceLayout.nullable=1;reject(module,target);*array=savedArray;
            array->typeValueId=0;reject(module,target);*array=savedArray;
            XL6_IrType* callback=(XL6_IrType*)xl6IrTypeGet(&module->types,array->params[0]);XL6_IrType savedCallback=*callback;
            callback->paramCount=1;reject(module,target);*callback=savedCallback;
            callback->returnCount=1;reject(module,target);*callback=savedCallback;
            callback->nullAbi=XL6_IR_NULL_ABI_XVALUE_SENTINEL;reject(module,target);*callback=savedCallback;
            callback->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;reject(module,target);*callback=savedCallback;
            callback->cAbi=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;reject(module,target);*callback=savedCallback;
            callback->typeValueId=0;reject(module,target);*callback=savedCallback;
            ++batches;
        }
        assert(batches&&refusals-before>=17&&xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
        printf("Executor batch XLIR: %zu additional pre-emission refusals; exact array/void-signature/layout/error/context/borrow contracts\n",refusals-before);
    }
    return 0;
}
