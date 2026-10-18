#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t refusals;
static void reject(XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(target)assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header<2;++header){
        XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_DIRECT_CHANNEL",&diagnostics):
            xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);
    }
    ++refusals;
}
static XL6_IrTypeId narrow(const XL6_IrModule* module)
{
    for(size_t i=0;i<module->types.count;++i)
        if(module->types.items[i].kind==XL6_IR_TYPE_BUILTIN&&
            module->types.items[i].integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_8)return (XL6_IrTypeId)(i+1);
    return XL6_IR_TYPE_INVALID;
}
int main(int argc,char** argv)
{
    assert(argc==4);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;size_t sends=0,receives=0,adopted=0,reads=0,typed=0,directions=0;
    assert(xl6IrVerifyModule(module,NULL));XL6_IrTypeId small=narrow(module);assert(small);
    assert(module->abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=module->callTargets+i;XL6_IrCallTarget saved=*target;
        if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_EXECUTOR_V1){
            assert(!target->nativeContextType&&target->nativeContextPayloadAbi==XL6_IR_ASYNC_RESULT_INVALID);
            target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_NONE;reject(module,target);*target=saved;
            target->nativeContextType=small;reject(module,target);*target=saved;
            XL6_IrType* future=(XL6_IrType*)xl6IrTypeGet(&module->types,target->returnType);
            assert(future&&future->paramCount==1&&future->params);XL6_IrTypeId element=future->params[0];
            future->params[0]=small;reject(module,target);future->params[0]=element;++sends;
            for(size_t j=0;j<module->callTargetCount;++j){
                const XL6_IrCallTarget* other=module->callTargets+j;
                if(other->nativeContextAbi!=XL6_IR_CALL_NATIVE_CONTEXT_EXECUTOR_CLASS_FIELD_V1)continue;
                XL6_IrTypeId receiving=other->parameterTypes[0];
                const XL6_IrClassField* field=xl6IrResidentNativeResourceField(module,receiving,"xl6ChannelReceiveFutureBuilderDestroyV1");
                if(!field)continue;
                XL6_IrTypeId input=target->parameterTypes[0];XL6_IrNodeId declaration=target->parameterPhysicalFieldDeclarations[0];
                target->parameterTypes[0]=receiving;target->parameterPhysicalFieldDeclarations[0]=field->declaration;
                reject(module,target);target->parameterTypes[0]=input;target->parameterPhysicalFieldDeclarations[0]=declaration;
                ++directions;break;
            }
        }
        if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_EXECUTOR_CLASS_FIELD_V1){
            assert(xl6IrNativeClassContextField(module,target));
            target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_NONE;reject(module,target);*target=saved;
            target->nativeContextPayloadAbi=XL6_IR_ASYNC_RESULT_INVALID;reject(module,target);*target=saved;
            XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[1];
            target->parameterPhysicalAbis[1]=XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW;
            reject(module,target);target->parameterPhysicalAbis[1]=physical;++receives;
        }
        if(target->resultPhysicalAbi==XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_HANDLE_VALUE_TAKE){
            target->resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE;reject(module,target);*target=saved;
            target->resultPhysicalDestroySymbol=0;reject(module,target);*target=saved;++adopted;
        }
        if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1&&
            target->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1){
            target->nativeResultAbi=XL6_IR_CALL_NATIVE_RESULT_NONE;reject(module,target);*target=saved;
            target->nativeContextType=0;reject(module,target);*target=saved;++reads;
        }
        if(target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1&&
            target->kind==XL6_IR_CALL_TARGET_NATIVE_FACTORY){
            for(size_t j=0;j<module->callTargetCount;++j){
                const XL6_IrCallTarget* other=module->callTargets+j;
                if(i==j||!other->cName||strcmp(target->cName,other->cName)||other->returnType==target->returnType)continue;
                const XL6_IrType* left=xl6IrTypeGet(&module->types,target->returnType);
                const XL6_IrType* right=xl6IrTypeGet(&module->types,other->returnType);
                if(!left||!right||left->paramCount!=1||right->paramCount!=1||left->params[0]==right->params[0])continue;
                target->returnType=other->returnType;reject(module,target);*target=saved;++typed;break;
            }
        }
    }
    assert(sends>=2&&receives>=2&&adopted&&reads&&typed&&directions&&refusals>=15);
    assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[2],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(!unit.ir.asyncMachineCount&&unit.ir.abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_FIXED_WORKERS_V1);
    unit.ir.abi.continuationAbi=XL6_IR_MODULE_CONTINUATION_NONE;reject(&unit.ir,NULL);xl6CompileUnitFree(&unit);
    xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[3],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(!unit.ir.asyncMachineCount&&unit.ir.abi.continuationAbi==XL6_IR_MODULE_CONTINUATION_NONE);
    assert(unit.ir.abi.executorOwnershipAbi==XL6_IR_MODULE_EXECUTOR_OWNERSHIP_NONE);xl6CompileUnitFree(&unit);
    printf("Direct Channel Future: %zu sends, %zu exact receives, %zu managed adoptions, %zu typed reads, %zu mismatched T, %zu wrong directions, %zu zero-output refusals\n",
        sends,receives,adopted,reads,typed,directions,refusals);return 0;
}
