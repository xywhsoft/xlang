#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t targets,refusals,bytesResults,policies;
static void reject(XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(target)assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;XL6_DiagList errors;size_t count=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_NET_FILE",&errors):xl6IrCgenEmitC(&out,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&out,&count);assert(!count);
        xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&errors);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    assert(module->abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1);
    assert(module->abi.taskPoolAbi==XL6_IR_MODULE_TASK_POOL_NONE);
    XL6_IrModuleAbi abi=module->abi;module->abi.executionOwnershipAbi=XL6_IR_MODULE_EXECUTION_OWNERSHIP_NONE;
    reject(module,NULL);module->abi=abi;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        if(!target->cName||strncmp(target->cName,"xl6NetFile",10))continue;
        XL6_IrCallTarget saved=*target;++targets;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        target->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;
        reject(module,target);*target=saved;
        target->effectAbi=XL6_IR_CALL_EFFECT_PURE;reject(module,target);*target=saved;
        target->executionBoundaryAbi=saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?
            XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        reject(module,target);*target=saved;
        target->nativeServicesRequired=saved.nativeServicesRequired?0:XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;
        reject(module,target);*target=saved;
        if(saved.nativeHandlePolicyCName){
            target->nativeHandlePolicyCName="wrongNetFilePolicy";reject(module,NULL);*target=saved;++policies;
        }
        if(strstr(saved.cName,"Read")){
            const XL6_IrType* future=xl6IrTypeGet(&module->types,saved.returnType);assert(future&&future->paramCount==1);
            XL6_IrType* element=(XL6_IrType*)xl6IrTypeGet(&module->types,future->params[0]);XL6_IrType original=*element;
            assert(element->cAbi==XL6_IR_C_ABI_MANAGED_BYTES);
            element->cAbi=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;reject(module,target);*element=original;++bytesResults;
        }
    }
    assert(targets>=11&&bytesResults>=2&&policies&&xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    printf("NetFile XLIR: %zu targets, %zu pre-emission refusals; exact results/effects/services/handle\n",targets,refusals);
    return 0;
}
