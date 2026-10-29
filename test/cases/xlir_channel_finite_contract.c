#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t refusals;
static void reject(XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(target)assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header<2;++header){
        XL6_Output out;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_FINITE",&errors):xl6IrCgenEmitC(&out,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&errors);
    }
    ++refusals;
}
static size_t check(const char* path,bool asynchronous)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    assert(module->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    assert(module->abi.deadlineOwnershipAbi==XL6_IR_MODULE_DEADLINE_OWNERSHIP_RESIDENT_BINDINGS_JOIN_V1);
    if(!asynchronous)assert(!module->asyncMachineCount);
    XL6_IrModuleAbi abi=module->abi;
    module->abi.deadlineOwnershipAbi=XL6_IR_MODULE_DEADLINE_OWNERSHIP_NONE;reject(module,NULL);module->abi=abi;
    module->abi.deadlineOwnershipAbi=(XL6_IrModuleDeadlineOwnershipAbi)99;reject(module,NULL);module->abi=abi;
    module->abi.executorOwnershipAbi=XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE;reject(module,NULL);module->abi=abi;
    size_t finite=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=module->callTargets+i;XL6_IrCallTarget saved=*target;
        if(target->nativeServicesRequired!=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        assert(xl6IrDynamicMethodCallTargetSupported(module,target));
        const unsigned bad[]={0,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1,~0u};
        for(size_t b=0;b<sizeof(bad)/sizeof(*bad);++b){
            target->nativeServicesRequired=bad[b];
            assert(!xl6IrDynamicMethodCallTargetSupported(module,target));
            reject(module,target);*target=saved;
        }
        target->nativeContextType=XL6_IR_TYPE_INVALID;
        if(saved.nativeContextAbi)reject(module,target);
        *target=saved;
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1){
            target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_NONE;reject(module,target);*target=saved;
            target->nativeContextPayloadAbi=XL6_IR_ASYNC_RESULT_INVALID;reject(module,target);*target=saved;
            XL6_IrType* owner=(XL6_IrType*)xl6IrTypeGet(&module->types,target->nativeContextType);
            if(owner->paramCount==1){owner->paramCount=0;reject(module,target);owner->paramCount=1;}
        }
        ++finite;
    }
    assert(finite&&xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);return finite;
}
int main(int argc,char** argv)
{
    assert(argc==5);size_t direct=check(argv[1],true),select=check(argv[2],true),ready=check(argv[3],false);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[4],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(unit.ir.abi.deadlineOwnershipAbi==XL6_IR_MODULE_DEADLINE_OWNERSHIP_NONE);
    unit.ir.abi.deadlineOwnershipAbi=XL6_IR_MODULE_DEADLINE_OWNERSHIP_RESIDENT_BINDINGS_JOIN_V1;reject(&unit.ir,NULL);
    unit.ir.abi.deadlineOwnershipAbi=XL6_IR_MODULE_DEADLINE_OWNERSHIP_NONE;
    assert(xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
    printf("finite Channel: direct=%zu select=%zu ready=%zu, %zu pre-emission refusals\n",direct,select,ready,refusals);
    return 0;
}
