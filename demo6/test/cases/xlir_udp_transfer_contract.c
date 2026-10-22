/* Closed public UDP transfer surface. Compiler only; no socket is created. */
#define main previousUdpAsyncContractMain
#include "xlir_udp_async_contract.c"
#undef main
#include <stdlib.h>
static const char* const selected[]={
    "xl6UdpMessageTextV1",
    "xl6UdpMessageBytesV1",
    "xl6UdpMessageBufferV1",
    "xl6UdpMessageRefBytesV1",
    "xl6UdpMessageRefPacketV1",
    "xl6UdpMessageRefsV1",
    "xl6UdpMessageDestroyV1",
    "xl6UdpMessageTargetV1",
    "xl6UdpMessageControlV1",
    "xl6UdpMessageNoTargetV1",
    "xl6UdpMessageNoControlV1",
    "xl6UdpMessageLengthV1",
    "xl6UdpMessageHasTargetV1",
    "xl6UdpMessageControlFlagsV1",
    "xl6UdpSendVectorTextV1",
    "xl6UdpSendVectorToTextV1",
    "xl6UdpSendVectorMessageTextV1",
    "xl6UdpSendVectorConnectedMessageTextV1",
    "xl6UdpSendVectorBytesV1",
    "xl6UdpSendVectorToBytesV1",
    "xl6UdpSendVectorMessageBytesV1",
    "xl6UdpSendVectorConnectedMessageBytesV1",
    "xl6UdpSendVectorBufferV1",
    "xl6UdpSendVectorToBufferV1",
    "xl6UdpSendVectorMessageBufferV1",
    "xl6UdpSendVectorConnectedMessageBufferV1",
    "xl6UdpSendRefsV1",
    "xl6UdpSendRefsToV1",
    "xl6UdpSendRefsMessageV1",
    "xl6UdpSendRefsConnectedMessageV1",
    "xl6UdpSendMessageV1",
    "xl6UdpSendMessagesV1",
    "xl6UdpSendPacketBatchV1",
    "xl6UdpMulticastJoinV1",
    "xl6UdpMulticastJoinAsyncV1",
    "xl6UdpMulticastJoinOnV1",
    "xl6UdpMulticastJoinOnAsyncV1",
    "xl6UdpMulticastLeaveV1",
    "xl6UdpMulticastLeaveAsyncV1",
    "xl6UdpMulticastLeaveOnV1",
    "xl6UdpMulticastLeaveOnAsyncV1",
    "xl6UdpMulticastLoopV1",
    "xl6UdpMulticastLoopAsyncV1",
    "xl6UdpMulticastHopLimitV1",
    "xl6UdpMulticastHopLimitAsyncV1",
    "xl6UdpMulticastInterfaceV1",
    "xl6UdpMulticastInterfaceAsyncV1",
    "xl6UdpMulticastInterfaceDefaultV1",
    "xl6UdpMulticastInterfaceDefaultAsyncV1",
};
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));XL6_IrTypeId integer=0;
    for(size_t i=0;i<m->types.count;++i)if(m->types.items[i].kind==XL6_IR_TYPE_BUILTIN&&m->types.items[i].cAbi==XL6_IR_C_ABI_I64&&m->types.items[i].integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;
    assert(integer);bool seen[sizeof(selected)/sizeof(*selected)]={0};size_t arrays=0,boolFutures=0;
    for(size_t i=0;i<m->callTargetCount;++i){
        XL6_IrCallTarget* t=&m->callTargets[i];size_t index=0;
        while(index<sizeof(selected)/sizeof(*selected)&&(!t->cName||strcmp(t->cName,selected[index])))++index;
        if(index==sizeof(selected)/sizeof(*selected))continue;
        assert(!seen[index]);seen[index]=true;++targets;exerciseEmission=true;
        XL6_IrCallTarget saved=*t;assert(xl6IrRuntimeDirectBridgeTargetMatches(m,t));
        #define BAD(Field,Value) do{t->Field=(Value);refused(m,t);*t=saved;}while(0)
        BAD(errorAbi,saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING);
        BAD(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1);
        BAD(nativeContextAbi,saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_NONE?XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1:XL6_IR_CALL_NATIVE_CONTEXT_NONE);
        BAD(returnType,integer);
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.resultPhysicalAbi!=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT){++resources;BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);BAD(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);BAD(resultPhysicalDestroySymbol,0);}
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1){++arrays;BAD(nativeContextType,0);}
        enumContract(m,t,t->returnType,integer);
        for(size_t p=0;p<t->parameterCount;++p){
            XL6_IrTypeId old=t->parameterTypes[p];const XL6_IrType* type=xl6IrTypeGet(&m->types,old);
            enumContract(m,t,old,integer);
            if(old!=integer){t->parameterTypes[p]=integer;refused(m,t);t->parameterTypes[p]=old;}
            if(type&&type->kind==XL6_IR_TYPE_CLASS&&!type->paramCount){
                XL6_IrCallParameterPhysicalAbi abi=t->parameterPhysicalAbis[p];
                t->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(m,t);t->parameterPhysicalAbis[p]=abi;
            }
            if(type&&type->collectionAbi==XL6_IR_COLLECTION_ARRAY){
                XL6_IrType* array=(XL6_IrType*)type;assert(array->paramCount==1);
                XL6_IrTypeId element=array->params[0];array->params[0]=integer;refused(m,t);array->params[0]=element;
            }
        }
        const XL6_IrType* result=xl6IrTypeGet(&m->types,t->returnType);
        if(result&&result->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR){
            XL6_IrType* future=(XL6_IrType*)result;assert(future->paramCount==1);
            XL6_IrTypeId old=future->params[0];future->params[0]=integer;refused(m,t);future->params[0]=old;++boolFutures;
        }
        assert(xl6IrVerifyModule(m,NULL));
        #undef BAD
    }
    for(size_t i=0;i<sizeof(selected)/sizeof(*selected);++i)if(!seen[i]){fprintf(stderr,"missing %s\n",selected[i]);abort();}
    assert(targets==49&&arrays==11&&boolFutures==8&&emissionRefusals==49&&refusals>400);
    xl6CompileUnitFree(&unit);
    printf("UDP transfer XLIR: %zu targets, %zu class arrays, %zu bool Futures, %zu enum positions, %zu owned results, %zu corrupt contracts refused, %zu whole-module/C/header refusals\n",targets,arrays,boolFutures,enums,resources,refusals,emissionRefusals);return 0;
}

