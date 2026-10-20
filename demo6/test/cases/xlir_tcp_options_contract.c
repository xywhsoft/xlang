/* Independent TCP option call/enum/scalar/owner mutations; no transport I/O. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t targets,refusals,enums,resources;
static void refused(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target)&&!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output output;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_TCP_OPTIONS",&errors):xl6IrCgenEmitC(&output,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&errors);}
    ++refusals;
}
static const char* names[]={
    "xl6TcpStreamOptionsCreateV1","xl6TcpStreamOptionsDestroyV1","xl6TcpStreamOptionsReadSizeV1","xl6TcpStreamOptionsReadLimitV1",
    "xl6TcpStreamOptionsWriteHighWaterV1","xl6TcpStreamOptionsWriteLowWaterV1","xl6TcpStreamOptionsWriteLimitV1",
    "xl6TcpStreamOptionsConnectTimeoutV1","xl6TcpStreamOptionsReadModeV1","xl6TcpStreamOptionsNoDelayV1","xl6TcpStreamOptionsKeepAliveV1",
    "xl6TcpStreamOptionsWithReadBufferV1","xl6TcpStreamOptionsWithWriteWatermarksV1","xl6TcpStreamOptionsWithConnectTimeoutV1",
    "xl6TcpStreamOptionsWithReadModeV1","xl6TcpStreamOptionsWithNoDelayV1","xl6TcpStreamOptionsWithKeepAliveV1",
    "xl6TcpDialOptionsCreateV1","xl6TcpDialOptionsDestroyV1","xl6TcpDialOptionsStreamV1","xl6TcpDialOptionsWithStreamV1",
    "xl6TcpDialOptionsFamilyV1","xl6TcpDialOptionsAffinityV1","xl6TcpDialOptionsTimeoutV1","xl6TcpDialOptionsFallbackDelayV1",
    "xl6TcpDialOptionsMaxAttemptsV1","xl6TcpDialOptionsWithFamilyV1","xl6TcpDialOptionsWithAffinityV1","xl6TcpDialOptionsWithTimeoutV1",
    "xl6TcpDialOptionsWithFallbackDelayV1","xl6TcpDialOptionsWithMaxAttemptsV1",
    "xl6TcpListenOptionsCreateV1","xl6TcpListenOptionsCreateAddressV1","xl6TcpListenOptionsDestroyV1","xl6TcpListenOptionsAddressV1",
    "xl6TcpListenOptionsWithAddressV1","xl6TcpListenOptionsStreamV1","xl6TcpListenOptionsWithStreamV1","xl6TcpListenOptionsAffinityV1",
    "xl6TcpListenOptionsAcceptConcurrencyV1","xl6TcpListenOptionsAcceptQueueLimitV1","xl6TcpListenOptionsBacklogV1",
    "xl6TcpListenOptionsDistributionV1","xl6TcpListenOptionsReuseAddressV1","xl6TcpListenOptionsReusePortV1",
    "xl6TcpListenOptionsExclusiveAddressV1","xl6TcpListenOptionsIPv6OnlyV1","xl6TcpListenOptionsWithAffinityV1",
    "xl6TcpListenOptionsWithAcceptQueueV1","xl6TcpListenOptionsWithBacklogV1","xl6TcpListenOptionsWithDistributionV1","xl6TcpListenOptionsWithBindingV1"};
static void enumContract(XL6_IrModule* m,XL6_IrCallTarget* target,XL6_IrTypeId id,XL6_IrTypeId integer)
{
    XL6_IrEnumLayout* layout=(XL6_IrEnumLayout*)xl6IrEnumLayoutGet(m,id);if(!layout)return;
    XL6_IrType* type=(XL6_IrType*)xl6IrTypeGet(&m->types,id);XL6_IrType saved=*type;++enums;
    type->nominalSymbol=0;refused(m,target);*type=saved;
    type->integerTarget=XL6_IR_INTEGER_TARGET_SIGNED_32;refused(m,target);*type=saved;
    size_t count=layout->variantCount;--layout->variantCount;refused(m,target);layout->variantCount=count;
    XL6_IrEnumVariant* member=&m->enumVariants[layout->firstVariant];char* name=member->name;
    member->name="notARegisteredTcpOptionMember";refused(m,target);member->name=name;
    XL6_IrTypeId* slot=target->returnType==id?&target->returnType:NULL;
    for(size_t p=0;!slot&&p<target->parameterCount;++p)if(target->parameterTypes[p]==id)slot=&target->parameterTypes[p];
    assert(slot);*slot=integer;refused(m,target);*slot=id;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));
    XL6_IrTypeId stream=0,dial=0,integer=0,unsignedInteger=0;
    for(size_t i=0;i<m->types.count;++i){const XL6_IrType* t=&m->types.items[i];
        if(t->kind==XL6_IR_TYPE_BUILTIN&&t->cAbi==XL6_IR_C_ABI_I64&&t->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;
        if(t->kind==XL6_IR_TYPE_BUILTIN&&t->cAbi==XL6_IR_C_ABI_U64&&t->integerTarget==XL6_IR_INTEGER_TARGET_UNSIGNED_64)unsignedInteger=(XL6_IrTypeId)i+1;}
    for(size_t i=0;i<m->callTargetCount;++i)if(m->callTargets[i].cName){
        if(!strcmp(m->callTargets[i].cName,"xl6TcpStreamOptionsCreateV1"))stream=m->callTargets[i].returnType;
        if(!strcmp(m->callTargets[i].cName,"xl6TcpDialOptionsCreateV1"))dial=m->callTargets[i].returnType;}
    assert(stream&&dial&&integer&&unsignedInteger&&sizeof(names)/sizeof(names[0])==52);uint64_t seen=0;
    for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* t=&m->callTargets[i];
        if(!t->cName||(strncmp(t->cName,"xl6TcpStreamOptions",sizeof("xl6TcpStreamOptions")-1)&&
            strncmp(t->cName,"xl6TcpDialOptions",sizeof("xl6TcpDialOptions")-1)&&
            strncmp(t->cName,"xl6TcpListenOptions",sizeof("xl6TcpListenOptions")-1)))continue;
        unsigned found=52;for(unsigned n=0;n<52;++n)if(!strcmp(names[n],t->cName))found=n;
        assert(found<52&&!(seen&(UINT64_C(1)<<found)));seen|=UINT64_C(1)<<found;
        XL6_IrCallTarget saved=*t;++targets;assert(xl6IrRuntimeDirectBridgeTargetMatches(m,t));
#define BAD(Field,Value) do{t->Field=(Value);refused(m,t);*t=saved;}while(0)
        BAD(errorAbi,saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE);
        BAD(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        BAD(executionBoundaryAbi,saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING);
        BAD(nativeServicesRequired,XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1);
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE)BAD(errorPhysicalAbi,XL6_IR_CALL_ERROR_PHYSICAL_NONE);
        if(saved.resultPhysicalAbi!=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT){++resources;BAD(resultPhysicalAbi,XL6_IR_CALL_RESULT_PHYSICAL_DIRECT);BAD(resultPhysicalFieldDeclaration,XL6_IR_NODE_INVALID);BAD(resultPhysicalDestroySymbol,0);}
        enumContract(m,t,t->returnType,integer);
        for(size_t p=0;p<t->parameterCount;++p){XL6_IrTypeId old=t->parameterTypes[p];enumContract(m,t,old,integer);
            const XL6_IrType* type=xl6IrTypeGet(&m->types,old);if(type&&type->kind==XL6_IR_TYPE_CLASS){
                XL6_IrCallParameterPhysicalAbi abi=t->parameterPhysicalAbis[p];t->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;refused(m,t);t->parameterPhysicalAbis[p]=abi;
                t->parameterTypes[p]=old==stream?dial:stream;refused(m,t);t->parameterTypes[p]=old;
            }else if(type&&type->kind==XL6_IR_TYPE_BUILTIN){t->parameterTypes[p]=old==integer?unsignedInteger:integer;refused(m,t);t->parameterTypes[p]=old;}}
        XL6_IrType* returned=(XL6_IrType*)xl6IrTypeGet(&m->types,saved.returnType);
        if(returned&&returned->kind==XL6_IR_TYPE_BUILTIN){t->returnType=saved.returnType==integer?unsignedInteger:integer;refused(m,t);t->returnType=saved.returnType;}
        assert(xl6IrVerifyModule(m,NULL));
#undef BAD
    }
    assert(targets==52&&seen==((UINT64_C(1)<<52)-1)&&resources==26&&enums==4&&refusals>400);
    xl6CompileUnitFree(&unit);printf("TCP options XLIR: %zu targets, %zu enum positions, %zu owned results, %zu corrupt contracts refused without C/header\n",targets,enums,resources,refusals);return 0;
}
