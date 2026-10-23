/* Exact X.509 profile factory/receiver contracts before module/C/header output. Offline only. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char* const entries[]={
    #define XL6_TEXT_EXPORT(Symbol,Provider) #Symbol,
    #include "src/runtime/xl6_x509_path_symbols.inc"
    #undef XL6_TEXT_EXPORT
};
_Static_assert(X509_ERROR_PATH==24 && X509_ERROR_NAME_CONSTRAINTS==44,"native reason vocabulary changed");
_Static_assert(X509_ERROR_TIME==7 && X509_ERROR_KEY_USAGE==15 && X509_ERROR_SIGNATURE_ALGORITHM==21,"precise native reason vocabulary changed");
static size_t refusals,targets,handles,resources,outputRefusals;
static void refused(XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(target){assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));++refusals;return;}
    ++outputRefusals;
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header) {
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_X509",&diagnostics):xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    XL6_IrTypeId integer=0,boolean=0;
    for(size_t i=0;i<module->types.count;++i) {
        const XL6_IrType* type=&module->types.items[i];
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_BOOL)boolean=(XL6_IrTypeId)i+1;
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_I64&&type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;
    }
    assert(integer&&boolean);bool seen[sizeof(entries)/sizeof(*entries)]={0};
    for(size_t i=0;i<module->callTargetCount;++i) {
        XL6_IrCallTarget* target=&module->callTargets[i];size_t index=0;
        while(index<sizeof(entries)/sizeof(*entries)&&(!target->cName||strcmp(entries[index],target->cName)))++index;
        if(index==sizeof(entries)/sizeof(*entries))continue;
        seen[index]=true;++targets;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        XL6_IrCallTarget saved=*target;
        #define BAD(Field,Value) do{target->Field=(Value);refused(module,target);*target=saved;}while(0)
        BAD(errorAbi,saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING);
        BAD(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1);
        BAD(nativeContextAbi,saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1?XL6_IR_CALL_NATIVE_CONTEXT_NONE:XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1);
        BAD(returnType,saved.returnType==integer?boolean:integer);
        if(saved.parameterCount)BAD(parameterCount,saved.parameterCount-1);
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.nativeHandlePolicyCName) {
            ++handles;target->nativeHandlePolicyCName="notAnRSAPolicy";refused(module,NULL);*target=saved;
        }
        if(saved.resultPhysicalAbi!=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT) {
            ++resources;BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);
            BAD(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);BAD(resultPhysicalDestroySymbol,0);
        }
        for(size_t p=0;p<target->parameterCount;++p) {
            XL6_IrTypeId original=target->parameterTypes[p];
            target->parameterTypes[p]=original==integer?boolean:integer;refused(module,target);target->parameterTypes[p]=original;
            const XL6_IrType* type=xl6IrTypeGet(&module->types,original);
            if(type&&type->kind==XL6_IR_TYPE_CLASS) {
                XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];
                target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(module,target);target->parameterPhysicalAbis[p]=physical;
                XL6_IrNodeId field=target->parameterPhysicalFieldDeclarations[p];
                target->parameterPhysicalFieldDeclarations[p]=XL6_IR_NODE_INVALID;refused(module,target);target->parameterPhysicalFieldDeclarations[p]=field;
            }
            if(type&&type->collectionAbi==XL6_IR_COLLECTION_ARRAY) {
                XL6_IrType* array=&module->types.items[original-1];
                XL6_IrCollectionAbi abi=array->collectionAbi;array->collectionAbi=XL6_IR_COLLECTION_LIST;
                refused(module,target);array->collectionAbi=abi;
            }
            if(type&&type->kind==XL6_IR_TYPE_FUNCTION) {
                XL6_IrType* callable=&module->types.items[original-1];size_t count=callable->paramCount;
                callable->paramCount=count+1;refused(module,target);callable->paramCount=count;
                XL6_IrTypeId result=callable->returns[0],physicalResult=callable->callableResultType;
                callable->returns[0]=integer;callable->callableResultType=integer;
                refused(module,target);callable->returns[0]=result;callable->callableResultType=physicalResult;
                XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[p];
                target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE;
                refused(module,target);target->parameterPhysicalAbis[p]=physical;
            }
            if(type&&type->kind==XL6_IR_TYPE_NUMERIC_ENUM) {
                const XL6_IrEnumLayout* layout=xl6IrEnumLayoutGet(module,original);assert(layout);
                XL6_IrEnumVariant* member=&module->enumVariants[layout->firstVariant];
                char* name=member->name;member->name="notACrlVersion";refused(module,target);member->name=name;
                XL6_IrEnumLayout* mutableLayout=&module->enumLayouts[layout-module->enumLayouts];
                size_t count=mutableLayout->variantCount;mutableLayout->variantCount=count-1;refused(module,target);mutableLayout->variantCount=count;
            }
        }
        const XL6_IrType* resultType=xl6IrTypeGet(&module->types,saved.returnType);
        if(resultType && resultType->kind==XL6_IR_TYPE_NUMERIC_ENUM) {
            const XL6_IrEnumLayout* layout=xl6IrEnumLayoutGet(module,saved.returnType);assert(layout);
            XL6_IrEnumVariant* member=&module->enumVariants[layout->firstVariant];
            char* name=member->name;member->name="notARecordMember";refused(module,target);member->name=name;
            XL6_IrEnumLayout* mutableLayout=&module->enumLayouts[layout-module->enumLayouts];
            size_t count=mutableLayout->variantCount;mutableLayout->variantCount=count-1;refused(module,target);mutableLayout->variantCount=count;
            XL6_IrType* mutableType=&module->types.items[saved.returnType-1];
            XL6_IrCAbi abi=mutableType->cAbi;mutableType->cAbi=XL6_IR_C_ABI_U64;refused(module,target);mutableType->cAbi=abi;
            for(size_t peer=0;peer<module->types.count;++peer) {
                if(peer+1!=saved.returnType && module->types.items[peer].kind==XL6_IR_TYPE_NUMERIC_ENUM) {
                    target->returnType=(XL6_IrTypeId)peer+1;refused(module,target);*target=saved;break;
                }
            }
        }
        target->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;
        refused(module,NULL);*target=saved;
        assert(xl6IrVerifyModule(module,NULL));
        #undef BAD
    }
    for(size_t i=0;i<sizeof(entries)/sizeof(*entries);++i){if(!seen[i])fprintf(stderr,"missing X509 target: %s\n",entries[i]);assert(seen[i]);}
    assert(targets>=32&&handles>=5&&resources>=11&&refusals>300&&outputRefusals>=32&&xl6IrVerifyModule(module,NULL));
    printf("X509 ordered path XLIR: %zu direct targets, %zu owned results, %zu bad contracts, %zu module/C/header output gates\n",
        targets,resources,refusals,outputRefusals);
    xl6CompileUnitFree(&unit);return 0;
}
