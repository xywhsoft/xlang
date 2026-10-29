/* Actual lowered contracts; mutation must fail before either C output. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t refusals;
static void reject(XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header<2;++header){
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_TASK_GROUP",&diagnostics):
            xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));size_t targets=0,borrows=0,factories=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=module->callTargets+i;XL6_IrCallTarget saved=*target;
        if(!target->cName || strncmp(target->cName,"xl6TaskGroup",12))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));++targets;
        target->effectAbi=XL6_IR_CALL_EFFECT_PURE;reject(module,target);*target=saved;
        target->executionBoundaryAbi=saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?
            XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        reject(module,target);*target=saved;
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE){
            target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;reject(module,target);*target=saved;
        }
        if(saved.resultPhysicalDestroySymbol){
            target->resultPhysicalDestroySymbol=0;reject(module,target);*target=saved;++factories;
            for(size_t j=0;j<module->callTargetCount;++j){
                XL6_SymbolId other=module->callTargets[j].resultPhysicalDestroySymbol;
                if(!other || other==saved.resultPhysicalDestroySymbol)continue;
                target->resultPhysicalDestroySymbol=other;reject(module,target);*target=saved;break;
            }
        }
        for(size_t p=0;p<target->parameterCount;++p){
            if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW)continue;
            XL6_IrNodeId declaration=target->parameterPhysicalFieldDeclarations[p];
            target->parameterPhysicalFieldDeclarations[p]=XL6_IR_NODE_INVALID;reject(module,target);
            target->parameterPhysicalFieldDeclarations[p]=declaration;++borrows;
        }
    }
    assert(targets>=28 && borrows>=20 && factories>=3 && xl6IrVerifyModule(module,NULL));
    xl6CompileUnitFree(&unit);
    printf("TaskGroup configured: %zu targets, %zu native borrows, %zu factories, %zu pre-emission refusals\n",
        targets,borrows,factories,refusals);return 0;
}
