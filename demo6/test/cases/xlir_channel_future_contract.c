#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t refusals;
static void reject(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header<2;++header){
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_CHANNEL_FUTURE",&diagnostics):
            xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(size==0);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==4);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;size_t adopted=0,started=0;
    assert(xl6IrVerifyModule(module,NULL));
    assert(module->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=module->callTargets+i;XL6_IrCallTarget saved=*target;
        if(target->resultPhysicalAbi==XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_HANDLE_VALUE_TAKE){
            assert(xl6IrRuntimeDirectBridgeManagedHandleResult(target->cName));
            assert(xl6IrCallTargetNativeClassResultField(module,target));
            XL6_IrTypeId physical=xl6IrCallTargetPhysicalResultType(module,target);
            assert(physical && physical!=target->returnType);
            assert(xl6IrTypeGet(&module->types,physical)->cAbi==XL6_IR_C_ABI_RUNTIME_XVALUE_PTR);
            target->resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE;reject(module);*target=saved;
            target->resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;reject(module);*target=saved;
            target->resultPhysicalDestroySymbol=0;reject(module);*target=saved;
            target->resultPhysicalFieldDeclaration=0;reject(module);*target=saved;
            ++adopted;
        }
        if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_EXECUTOR_CLASS_FIELD_V1){
            assert(xl6IrNativeClassContextField(module,target));
            assert(target->nativeContextPayloadAbi==XL6_IR_ASYNC_RESULT_RETAINED_XVALUE);
            target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_NONE;reject(module);*target=saved;
            target->nativeContextType=0;reject(module);*target=saved;
            target->nativeContextPayloadAbi=XL6_IR_ASYNC_RESULT_INVALID;reject(module);*target=saved;
            target->parameterPhysicalAbis[1]=XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW;
            reject(module);target->parameterPhysicalAbis[1]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
            ++started;
        }
    }
    assert(adopted && started>=2 && refusals>=12);
    assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[3],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));module=&unit.ir;
    assert(module->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_NONE);
    assert(module->abi.executorOwnershipAbi==XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE);
    assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[2],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));module=&unit.ir;
    assert(module->asyncMachineCount==0);
    assert(module->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    module->abi.continuationAbi=XL6_IR_MODULE_CONTINUATION_NONE;reject(module);
    module->abi.continuationAbi=XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1;
    assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    printf("Channel Future: %zu managed adoptions, %zu sealed starts, %zu refusals\n",adopted,started,refusals);
    return 0;
}
