/* Exact identity targets, nullable ownership and output rejection. Offline. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char* const entries[]={
    #define XL6_TEXT_EXPORT(Symbol,Provider) #Symbol,
    #include "src/runtime/xl6_x509_identity_symbols.inc"
    #undef XL6_TEXT_EXPORT
};
static size_t targets,owned,refusals,outputs,nullable;
static void refused(XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    ++refusals;
    if(target){assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));return;}
    ++outputs;assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;XL6_DiagList diags;size_t size=SIZE_MAX;xl6DiagInit(&diags);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_IDENTITY",&diags):xl6IrCgenEmitC(&out,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&diags);
    }
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));XL6_IrTypeId integer=0;
    for(size_t i=0;i<module->types.count;++i)
        if(module->types.items[i].kind==XL6_IR_TYPE_BUILTIN && module->types.items[i].cAbi==XL6_IR_C_ABI_I64 && module->types.items[i].integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)
            integer=(XL6_IrTypeId)i+1;
    assert(integer);bool seen[sizeof(entries)/sizeof(*entries)]={0};
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];size_t index=0;
        while(index<sizeof(entries)/sizeof(*entries) && (!target->cName || strcmp(entries[index],target->cName)))++index;
        if(index==sizeof(entries)/sizeof(*entries))continue;
        seen[index]=true;++targets;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));XL6_IrCallTarget saved=*target;
        #define BAD(Field,Value) do{target->Field=(Value);refused(module,target);*target=saved;}while(0)
        BAD(errorAbi,XL6_IR_ERROR_ABI_NO_RAISE);BAD(effectAbi,XL6_IR_CALL_EFFECT_CONSERVATIVE);
        BAD(executionBoundaryAbi,XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK);BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        BAD(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1);BAD(returnType,integer);BAD(parameterCount,saved.parameterCount-1);
        for(size_t p=0;p<target->parameterCount;++p){
            XL6_IrTypeId original=target->parameterTypes[p];const XL6_IrType* type=xl6IrTypeGet(&module->types,original);
            if(original!=integer){target->parameterTypes[p]=integer;refused(module,target);target->parameterTypes[p]=original;}
            if(type && type->kind==XL6_IR_TYPE_CLASS){
                XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module,target);target->parameterPhysicalAbis[p]=physical;
                XL6_IrNodeId field=target->parameterPhysicalFieldDeclarations[p];target->parameterPhysicalFieldDeclarations[p]=XL6_IR_NODE_INVALID;refused(module,target);target->parameterPhysicalFieldDeclarations[p]=field;
            }
        }
        const XL6_IrType* result=xl6IrTypeGet(&module->types,saved.returnType);
        if(result && result->kind==XL6_IR_TYPE_NULLABLE){assert(result->paramCount==1);++nullable;BAD(returnType,result->params[0]);}
        if(saved.resultPhysicalAbi!=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT){++owned;BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);BAD(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);BAD(resultPhysicalDestroySymbol,0);}
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module,NULL);*target=saved;assert(xl6IrVerifyModule(module,NULL));
        #undef BAD
    }
    for(size_t i=0;i<sizeof(entries)/sizeof(*entries);++i)assert(seen[i]);
    assert(targets==3 && owned==1 && nullable==1 && refusals>30 && outputs==3);
    printf("X509 identity XLIR: %zu direct targets, %zu owned result, %zu nullable result, %zu bad contracts, %zu output gates\n",targets,owned,nullable,refusals,outputs);
    xl6CompileUnitFree(&unit);return 0;
}
