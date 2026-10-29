#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t targets,owned,byteResults,arrays,handles,refusals,outputs,sameAbi;
static void refuse(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    refusals++;if(target){assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));return;}
    outputs++;assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;header++){
        XL6_Output out;XL6_DiagList diags;size_t size=SIZE_MAX;xl6DiagInit(&diags);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_TLS_ROLE",&diags):xl6IrCgenEmitC(&out,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&diags);
    }
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));XL6_IrTypeId integer=0,boolean=0,client=0,server=0;
    for(size_t i=0;i<module->types.count;i++){
        const XL6_IrType* type=&module->types.items[i];
        if(type->kind==XL6_IR_TYPE_BUILTIN && type->cAbi==XL6_IR_C_ABI_BOOL)boolean=(XL6_IrTypeId)i+1;
        if(type->kind==XL6_IR_TYPE_BUILTIN && type->cAbi==XL6_IR_C_ABI_I64 && type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;
    }
    for(size_t i=0;i<module->callTargetCount;i++){
        XL6_IrCallTarget* target=&module->callTargets[i];if(!target->cName)continue;
        if(!strcmp(target->cName,"xl6TlsClientContextDefaultV1"))client=target->returnType;
        if(!strcmp(target->cName,"xl6TlsServerContextDefaultV1"))server=target->returnType;
    }assert(integer && boolean && client && server && client!=server);
    for(size_t i=0;i<module->callTargetCount;i++){
        XL6_IrCallTarget* target=&module->callTargets[i];const char* name=target->cName;
        if(!name || (strncmp(name,"xl6TlsClientContext",strlen("xl6TlsClientContext")) && strncmp(name,"xl6TlsServerContext",strlen("xl6TlsServerContext"))))continue;
        targets++;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));XL6_IrCallTarget saved=*target;
#define BAD(Field,Value) do{target->Field=(Value);refuse(module,target);*target=saved;}while(0)
        BAD(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK);
        BAD(returnType,saved.returnType==integer?boolean:integer);BAD(parameterCount,saved.parameterCount+1);
        BAD(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1);
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.nativeHandlePolicyCName){handles++;target->nativeHandlePolicyCName="notATlsRolePolicy";refuse(module,NULL);*target=saved;}
        const XL6_IrType* result=xl6IrTypeGet(&module->types,saved.returnType);
        if(result && result->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)byteResults++;
        if(saved.resultPhysicalDestroySymbol){
            owned++;BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);
            target->resultPhysicalDestroySymbol=0;refuse(module,NULL);*target=saved;
        }
        for(size_t p=0;p<saved.parameterCount;p++){
            XL6_IrTypeId id=target->parameterTypes[p];const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
            target->parameterTypes[p]=id==integer?boolean:integer;refuse(module,target);target->parameterTypes[p]=id;
            if(id==client || id==server){
                sameAbi++;target->parameterTypes[p]=id==client?server:client;refuse(module,target);target->parameterTypes[p]=id;
            }
            if(type && type->collectionAbi==XL6_IR_COLLECTION_ARRAY){
                arrays++;XL6_IrType* array=&module->types.items[id-1];assert(array->paramCount==1);XL6_IrTypeId item=array->params[0];
                array->params[0]=integer;refuse(module,target);array->params[0]=item;
                XL6_IrCollectionAbi abi=array->collectionAbi;array->collectionAbi=XL6_IR_COLLECTION_LIST;refuse(module,target);array->collectionAbi=abi;
            }
        }
        target->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;refuse(module,NULL);*target=saved;
        assert(xl6IrVerifyModule(module,NULL));
        fprintf(stderr,"TLS role target %zu complete: %s\n",targets,name);fflush(stderr);
#undef BAD
    }
    assert(targets==21 && owned==14 && byteResults==2 && arrays==2 && handles==2 && sameAbi>10 && refusals>200);
    printf("TLS role XLIR: %zu targets; %zu native owned; %zu bytes; %zu exact bytes arrays; %zu policies; %zu same-representation role swaps; %zu rejected contracts; %zu output gates\n",targets,owned,byteResults,arrays,handles,sameAbi,refusals,outputs);
    xl6CompileUnitFree(&unit);return 0;
}
