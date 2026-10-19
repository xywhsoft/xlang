#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t refusals;
static void reject(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target)&&!xl6IrVerifyModule(module,NULL));
    for(int header=0;header<2;++header){
        XL6_Output output;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_TYPED_START",&errors):
            xl6IrCgenEmitC(&output,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&errors);
    }
    ++refusals;
}
static size_t check(const char* path)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));size_t count=0,relations=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=module->callTargets+i;
        if(!target->cName||strcmp(target->cName,"xl6TaskGroupStartLanguageV1"))continue;
        XL6_IrCallTarget saved=*target;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        assert(target->parameterCount==2&&target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1);
        target->nativeContextType=XL6_IR_TYPE_INVALID;reject(module,target);*target=saved;
        target->nativeContextPayloadAbi=XL6_IR_ASYNC_RESULT_INVALID;reject(module,target);*target=saved;
        target->executionBoundaryAbi=XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;reject(module,target);*target=saved;
        target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;reject(module,target);*target=saved;
        XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[1];
        assert(physical==XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW);
        target->parameterPhysicalAbis[1]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;reject(module,target);
        target->parameterPhysicalAbis[1]=physical;
        XL6_IrType* callback=(XL6_IrType*)xl6IrTypeGet(&module->types,target->parameterTypes[1]);
        XL6_IrType copy=*callback;assert(callback->returnCount==1&&callback->returns&&callback->returns[0]==target->returnType);
        callback->paramCount=1;reject(module,target);*callback=copy;
        callback->callableResultType=XL6_IR_TYPE_INVALID;reject(module,target);*callback=copy;
        callback->returnCount=0;reject(module,target);*callback=copy;
        XL6_IrType* future=(XL6_IrType*)xl6IrTypeGet(&module->types,target->returnType);XL6_IrType futureCopy=*future;
        future->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;reject(module,target);*future=futureCopy;
        future->nullAbi=XL6_IR_NULL_ABI_NONE;reject(module,target);*future=futureCopy;
        for(size_t t=0;t<module->types.count;++t){
            const XL6_IrType* other=module->types.items+t;XL6_IrTypeId id=(XL6_IrTypeId)(t+1);
            if(id==target->returnType||other->kind!=XL6_IR_TYPE_GENERIC_INSTANCE||
                other->cAbi!=XL6_IR_C_ABI_RUNTIME_FUTURE_PTR||other->paramCount!=1||!other->params||
                other->params[0]==future->params[0])continue;
            XL6_IrTypeId returned=callback->returns[0];
            callback->returns[0]=id;callback->callableResultType=id;
            reject(module,target);callback->returns[0]=returned;*callback=copy;++relations;break;
        }
        ++count;
    }
    assert(count&&relations==count&&xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);return count;
}
int main(int argc,char** argv)
{
    assert(argc==4);size_t count=0;
    for(int i=1;i<argc;++i)count+=check(argv[i]);
    printf("TaskGroup start: %zu exact typed callable/Future relations, %zu zero-C/header refusals\n",count,refusals);return 0;
}
