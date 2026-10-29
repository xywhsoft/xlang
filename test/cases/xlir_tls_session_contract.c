#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t targets,owned,products,bytesResults,policies,rejected,gates,sameAbi;
static void refuse(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    rejected++;if(target){assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));return;}
    gates++;assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;header++){
        XL6_Output out;XL6_DiagList diags;size_t size=SIZE_MAX;xl6DiagInit(&diags);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_TLS_SESSION",&diags):xl6IrCgenEmitC(&out,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&diags);
    }
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));XL6_IrTypeId integer=0,boolean=0,session=0,client=0,server=0;
    for(size_t i=0;i<module->types.count;i++){
        const XL6_IrType* type=&module->types.items[i];
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_BOOL)boolean=(XL6_IrTypeId)i+1;
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_I64&&type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;
    }
    for(size_t i=0;i<module->callTargetCount;i++){
        XL6_IrCallTarget* t=&module->callTargets[i];if(!t->cName)continue;
        if(!strcmp(t->cName,"xl6TlsSessionClientCreateV1")){session=t->returnType;client=t->parameterTypes[0];}
        if(!strcmp(t->cName,"xl6TlsSessionServerCreateV1"))server=t->parameterTypes[0];
    }assert(integer&&boolean&&session&&client&&server&&client!=server&&session!=client);
    for(size_t i=0;i<module->callTargetCount;i++){
        XL6_IrCallTarget* t=&module->callTargets[i];const char* name=t->cName;
        if(!name||strncmp(name,"xl6TlsSession",strlen("xl6TlsSession")))continue;
        targets++;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,t));XL6_IrCallTarget saved=*t;
#define BAD(Field,Value) do{t->Field=(Value);refuse(module,t);*t=saved;}while(0)
        BAD(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK?XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING:XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK);
        BAD(returnType,saved.returnType==integer?boolean:integer);BAD(parameterCount,saved.parameterCount+1);
        BAD(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1);
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.nativeHandlePolicyCName){policies++;t->nativeHandlePolicyCName="notATlsSessionPolicy";refuse(module,NULL);*t=saved;}
        const XL6_IrType* result=xl6IrTypeGet(&module->types,saved.returnType);
        if(result&&result->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)bytesResults++;
        if(result&&result->cAbi==XL6_IR_C_ABI_PRODUCT){
            products++;XL6_IrType* product=&module->types.items[saved.returnType-1];XL6_IrNativeProductAbi abi=product->nativeProductAbi;
            product->nativeProductAbi=XL6_IR_NATIVE_PRODUCT_NONE;refuse(module,NULL);product->nativeProductAbi=abi;
            if(abi==XL6_IR_NATIVE_PRODUCT_I64BYTES_V1){
                assert(product->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED);
                XL6_IrOwnershipAbi original=product->ownershipAbi;product->ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;refuse(module,NULL);product->ownershipAbi=original;
                XL6_IrTypeId element=product->params[1];product->params[1]=integer;refuse(module,t);product->params[1]=element;
            }
            if(abi==XL6_IR_NATIVE_PRODUCT_BOOLI64I64_V1){
                XL6_IrTypeId element=product->params[2];product->params[2]=boolean;refuse(module,t);product->params[2]=element;
                element=product->params[0];product->params[0]=integer;refuse(module,NULL);product->params[0]=element;
                XL6_IrOwnershipAbi original=product->ownershipAbi;product->ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED;refuse(module,NULL);product->ownershipAbi=original;
            }
        }
        if(saved.resultPhysicalDestroySymbol){owned++;BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);t->resultPhysicalDestroySymbol=0;refuse(module,NULL);*t=saved;}
        for(size_t p=0;p<saved.parameterCount;p++){
            XL6_IrTypeId id=t->parameterTypes[p];t->parameterTypes[p]=id==integer?boolean:integer;refuse(module,t);t->parameterTypes[p]=id;
            if(id==session||id==client||id==server){sameAbi++;t->parameterTypes[p]=id==client?server:client;refuse(module,t);t->parameterTypes[p]=id;}
        }
        t->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;refuse(module,NULL);*t=saved;
        assert(xl6IrVerifyModule(module,NULL));fprintf(stderr,"TLS session target %zu complete: %s\n",targets,name);fflush(stderr);
#undef BAD
    }
    assert(targets==31&&owned==3&&products==4&&bytesResults==3&&policies==1&&sameAbi>25&&rejected>290&&gates>35);
    printf("TLS session XLIR: %zu targets; %zu native owned; %zu products; %zu bytes; %zu policy; %zu same-representation swaps; %zu rejected contracts; %zu output gates\n",targets,owned,products,bytesResults,policies,sameAbi,rejected,gates);
    xl6CompileUnitFree(&unit);return 0;
}
