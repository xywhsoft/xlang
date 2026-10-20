/* Closed contracts only. The input source is compiled, never executed. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals,targets,products,arrays,fields,managed;
static void refused(XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_VECTOR_BATCH",&diagnostics):
            xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
    ++refusals;
}

static bool selected(const char* name)
{
    return name && (!strncmp(name,"xl6NetDatagramPacket",20) ||
        !strncmp(name,"xl6NetBatchProgress",19) ||
        (!strncmp(name,"xl6NetSocket",12) && (strstr(name,"Vector") || strstr(name,"Batch"))));
}

int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* module=&unit.ir;
    assert(xl6IrVerifyModule(module,NULL));
    XL6_IrTypeId integer=XL6_IR_TYPE_INVALID,packet=XL6_IR_TYPE_INVALID,address=XL6_IR_TYPE_INVALID;
    for(size_t i=0;i<module->types.count;++i){const XL6_IrType* type=&module->types.items[i];
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_I64&&
            type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;}
    for(size_t i=0;i<module->callTargetCount;++i){const XL6_IrCallTarget* target=&module->callTargets[i];
        if(!target->cName)continue;
        if(!strcmp(target->cName,"xl6NetDatagramPacketCreateV1"))packet=target->returnType;
        if(!strcmp(target->cName,"xl6NetAddressLoopbackV1"))address=target->returnType;
    }
    assert(integer&&packet&&address&&packet!=address);
    for(size_t i=0;i<module->callTargetCount;++i){XL6_IrCallTarget* target=&module->callTargets[i];
        if(!selected(target->cName))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        XL6_IrCallTarget saved=*target;++targets;
#define BAD(field,value) do{target->field=(value);refused(module,target);*target=saved;}while(0)
        BAD(errorAbi,saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(effectAbi,XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?
            XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING);
        BAD(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1);
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.nativeContextAbi!=XL6_IR_CALL_NATIVE_CONTEXT_NONE){
            BAD(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_NONE);
            BAD(nativeContextType,integer);
            BAD(nativeContextPayloadAbi,XL6_IR_ASYNC_RESULT_COPY_I64);
        }
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1){
            ++arrays;
            const XL6_IrClassField* field=xl6IrNativeClassArrayContextField(module,target);assert(field);
            XL6_IrClassField* change=(XL6_IrClassField*)field;XL6_IrClassField original=*change;
            change->nativeHandlePolicyCName="UnregisteredVectorPolicy";refused(module,target);*change=original;
            XL6_IrType* array=(XL6_IrType*)xl6IrTypeGet(&module->types,saved.nativeContextType);assert(array&&array->paramCount==1);
            XL6_IrTypeId element=array->params[0];array->params[0]=integer;refused(module,target);array->params[0]=element;
        }
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1)++fields;
        if(!strcmp(target->cName,"xl6NetDatagramPacketBufferHandleV1")){
            assert(saved.resultPhysicalAbi==XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_HANDLE_VALUE_TAKE);
            assert(saved.resultPhysicalFieldDeclaration&&saved.resultPhysicalDestroySymbol);++managed;
            BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE);
            BAD(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);
            BAD(resultPhysicalDestroySymbol,0);
        }
        for(size_t p=0;p<target->parameterCount;++p){
            XL6_IrTypeId type=target->parameterTypes[p];
            const XL6_IrType* item=xl6IrTypeGet(&module->types,type);
            if(item&&item->collectionAbi==XL6_IR_COLLECTION_ARRAY){
                target->parameterTypes[p]=integer;refused(module,target);target->parameterTypes[p]=type;
                XL6_IrParameterAbi abi=target->parameterAbis[p];
                target->parameterAbis[p]=XL6_IR_PARAMETER_ABI_VALUE;refused(module,target);target->parameterAbis[p]=abi;
            }else if(address&&type==packet){
                target->parameterTypes[p]=address;refused(module,target);target->parameterTypes[p]=type;
            }
        }
        XL6_IrType* tuple=(XL6_IrType*)xl6IrTypeGet(&module->types,saved.returnType);
        if(tuple&&tuple->nativeProductAbi==XL6_IR_NATIVE_PRODUCT_I64U64_V1){
            ++products;XL6_IrTypeId status=tuple->params[0];tuple->params[0]=integer;
            refused(module,target);tuple->params[0]=status;
        }
        assert(xl6IrVerifyModule(module,NULL));
#undef BAD
    }
    assert(targets==37&&products==16&&arrays==8&&fields==1&&managed==1&&refusals>200);
    xl6CompileUnitFree(&unit);
    printf("Vector/batch XLIR: %zu targets, %zu products, %zu class arrays, %zu fields, %zu managed results, %zu corrupt contracts refused\n",
        targets,products,arrays,fields,managed,refusals);return 0;
}
