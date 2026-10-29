/* Compiler contract checks only: no socket or thread is created. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t targets,refusals,contexts,byteFutures,policies,emissionGuards,deadlines,arrays,intFutures;
static unsigned guardedKinds;
static void refused(XL6_IrModule* module,const XL6_IrCallTarget* target,unsigned kind)
{
    if(target)assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    ++refusals;
    /* Every descriptor mutation is checked independently. The common full
     * verifier and both emission gates are checked once per violation kind,
     * instead of re-verifying the same large module three times per target. */
    unsigned bit=1u<<kind;
    if(guardedKinds&bit){if(!target)assert(!xl6IrVerifyModule(module,NULL));return;}
    guardedKinds|=bit;++emissionGuards;assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_TCP",&diagnostics):
            xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(size==0);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
}
static void check(const char* path)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    assert(module->abi.executionOwnershipAbi==XL6_IR_MODULE_EXECUTION_OWNERSHIP_RESIDENT_SOURCE_V1);
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        if(!target->cName||strncmp(target->cName,"xl6Tcp",6))continue;
        XL6_IrCallTarget saved=*target;++targets;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        target->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?
            XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;
        refused(module,target,0);*target=saved;
        target->effectAbi=saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?
            XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE;
        refused(module,target,1);*target=saved;
        target->executionBoundaryAbi=saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?
            XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        refused(module,target,2);*target=saved;
        target->nativeServicesRequired=saved.nativeServicesRequired?0:XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;
        refused(module,target,3);*target=saved;
        if(saved.nativeServicesRequired&XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1){
            target->nativeServicesRequired=saved.nativeServicesRequired&~XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1;
            refused(module,target,8);*target=saved;++deadlines;
        }
        if(saved.nativeHandlePolicyCName){
            ++policies;target->nativeHandlePolicyCName="wrongTcpHandlePolicy";
            refused(module,NULL,4);*target=saved;
        }
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1){
            ++contexts;target->nativeContextType=0;refused(module,target,5);*target=saved;
            target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_NONE;refused(module,target,6);*target=saved;
        }
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1){
            ++arrays;target->nativeContextType=0;refused(module,target,9);*target=saved;
            const XL6_IrClassField* field=xl6IrNativeClassArrayContextField(module,target);assert(field);
            XL6_IrClassField* change=(XL6_IrClassField*)field;XL6_IrClassField original=*change;
            change->nativeHandlePolicyCName="WrongTcpArrayPolicy";refused(module,NULL,9);*change=original;
        }
        if(strstr(saved.cName,"RecvInto")&&strstr(saved.cName,"Async")){
            XL6_IrType* future=(XL6_IrType*)xl6IrTypeGet(&module->types,saved.returnType);
            assert(future&&future->paramCount==1);XL6_IrTypeId original=future->params[0],boolean=0;
            const XL6_IrType* element=xl6IrTypeGet(&module->types,original);assert(element&&element->cAbi==XL6_IR_C_ABI_I64);
            for(size_t n=0;n<module->types.count;++n)if(module->types.items[n].cAbi==XL6_IR_C_ABI_BOOL){boolean=(XL6_IrTypeId)n+1;break;}
            assert(boolean);future->params[0]=boolean;refused(module,target,10);future->params[0]=original;++intFutures;
        }
        if(!strcmp(saved.cName,"xl6TcpStreamRecvAsyncV1")){
            const XL6_IrType* future=xl6IrTypeGet(&module->types,saved.returnType);
            assert(future&&future->paramCount==1);
            XL6_IrType* element=(XL6_IrType*)xl6IrTypeGet(&module->types,future->params[0]);
            XL6_IrType original=*element;assert(element->cAbi==XL6_IR_C_ABI_MANAGED_BYTES);
            /* Same retained-xvalue result codec is not sufficient authority
             * to change bytes into an arbitrary dynamic value. */
            element->cAbi=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;refused(module,target,7);*element=original;
            ++byteFutures;
        }
    }
    assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
}
int main(int argc,char** argv)
{
    assert(argc==3);check(argv[1]);check(argv[2]);
    assert(targets>50&&refusals>200&&contexts>=2&&policies>=4&&byteFutures>=1&&
        emissionGuards==(deadlines?9u:8u)+(arrays?1u:0u)+(intFutures?1u:0u));
    printf("TCP XLIR: %zu targets, %zu descriptor refusals, %zu violation kinds guarded by full verifier before C/header output; exact services, class fields, byte Futures, ownership and effects\n",targets,refusals,emissionGuards);
    return 0;
}
