/* Source is compiled/lowered only; no queries or network tests are executed. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t targets,refusals,arrays,futures,services;
static void refused(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output output;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_RESOLVER",&errors):xl6IrCgenEmitC(&output,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&errors);}
    ++refusals;
}
static bool selected(const char* name)
{return name&&(!strncmp(name,"xl6NetResolver",sizeof("xl6NetResolver")-1)||
    !strncmp(name,"xl6NetResolveOperation",sizeof("xl6NetResolveOperation")-1)||
    !strncmp(name,"xl6NetAddressList",sizeof("xl6NetAddressList")-1));}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    XL6_IrTypeId integer=0,options=0;
    for(size_t i=0;i<module->types.count;++i){const XL6_IrType* type=module->types.items+i;
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_I64&&type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;}
    for(size_t i=0;i<module->callTargetCount;++i){XL6_IrCallTarget* target=module->callTargets+i;
        if(target->cName&&!strcmp(target->cName,"xl6NetResolverConfigCreateV1"))options=target->returnType;}
    assert(integer&&options);
    for(size_t i=0;i<module->callTargetCount;++i){XL6_IrCallTarget* target=module->callTargets+i;
        if(!selected(target->cName))continue;
        XL6_IrCallTarget saved=*target;++targets;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
#define BAD(Field,Value) do{target->Field=(Value);refused(module,target);*target=saved;}while(0)
        BAD(errorAbi,saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?
            XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING);
        BAD(nativeServicesRequired,saved.nativeServicesRequired?0:XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1);
        if(saved.nativeServicesRequired)++services;
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.resultPhysicalAbi!=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT){
            BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);
            BAD(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);BAD(resultPhysicalDestroySymbol,0);}
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1){
            ++arrays;BAD(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_NONE);BAD(nativeContextType,integer);
            XL6_IrClassField* field=(XL6_IrClassField*)xl6IrNativeClassArrayContextField(module,target);assert(field);
            const char* policy=field->nativeHandlePolicyCName;field->nativeHandlePolicyCName="UnknownResolverPolicy";
            refused(module,target);field->nativeHandlePolicyCName=policy;}
        const XL6_IrType* returned=xl6IrTypeGet(&module->types,saved.returnType);
        if(returned&&returned->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR){
            ++futures;XL6_IrType* future=(XL6_IrType*)returned;assert(future->paramCount==1);
            XL6_IrTypeId original=future->params[0];future->params[0]=integer;refused(module,target);future->params[0]=original;}
        for(size_t p=0;p<target->parameterCount;++p){
            XL6_IrTypeId old=target->parameterTypes[p];const XL6_IrType* type=xl6IrTypeGet(&module->types,old);
            if(type&&type->kind==XL6_IR_TYPE_CLASS){
                XL6_IrCallParameterPhysicalAbi abi=target->parameterPhysicalAbis[p];
                target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module,target);target->parameterPhysicalAbis[p]=abi;
                if(old!=options){target->parameterTypes[p]=options;refused(module,target);target->parameterTypes[p]=old;}}
        }
        assert(xl6IrVerifyModule(module,NULL));
#undef BAD
    }
    assert(targets==61&&arrays==1&&futures==1&&services==2&&refusals>300);xl6CompileUnitFree(&unit);
    printf("Resolver XLIR: %zu targets, %zu arrays, %zu exact Future, %zu services, %zu corrupt contracts refused without C/header\n",targets,arrays,futures,services,refusals);
    return 0;
}
