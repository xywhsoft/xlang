/* Closed ABI mutations only. Never creates endpoints or network traffic. */
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t refusals,targets,products,owners,enums;
static void refused(XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(m,t)&&!xl6IrVerifyModule(m,NULL));
    for(unsigned header=0;header<2;++header){XL6_Output output;XL6_DiagList diagnostics;size_t size=SIZE_MAX;
        xl6DiagInit(&diagnostics);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,m,"BAD_DATAGRAM",&diagnostics):xl6IrCgenEmitC(&output,m,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&diagnostics);}
    ++refusals;
}
static bool selected(const char* name)
{
    if(!name)return false;
    #if defined(TEST_DATAGRAM_ERROR_ONLY)
    if(!strncmp(name,"xl6NetDatagramErrorInfo",23))return true;
    const char* socketNames[]={"ReceiveErrorInto","PathMtuModeSet","PathMtuMode","PathMtu","DatagramErrorsSet","DatagramErrors"};
    #else
    /* Packet has an independent complete vector/batch contract gate. Keep
     * this legacy value/control/IO selection exact rather than silently
     * counting new Packet methods as already covered by this gate. */
    if(!strncmp(name,"xl6NetDatagramPacket",20))return false;
    if(!strncmp(name,"xl6NetDatagram",14))return true;
    const char* socketNames[]={"MetadataAvailable","MetadataEnabled","ControlAvailable","DatagramCapabilities","MetadataSet",
        "MulticastJoin","MulticastJoinOn","MulticastLeave","MulticastLeaveOn","MulticastLoop","MulticastHopLimit",
        "MulticastInterface","MulticastInterfaceDefault","ReceiveFromInto","SendMessageText","SendMessageBytes","SendMessageBuffer",
        "SendConnectedMessageText","SendConnectedMessageBytes","SendConnectedMessageBuffer"};
    #endif
    if(strncmp(name,"xl6NetSocket",12))return false;
    for(size_t i=0;i<sizeof(socketNames)/sizeof(*socketNames);++i){char full[96];
        snprintf(full,sizeof(full),"xl6NetSocket%sV1",socketNames[i]);if(!strcmp(name,full))return true;}
    return false;
}
static void enumContract(XL6_IrModule* m,XL6_IrCallTarget* target,XL6_IrTypeId id,XL6_IrTypeId integer)
{
    XL6_IrEnumLayout* layout=(XL6_IrEnumLayout*)xl6IrEnumLayoutGet(m,id);if(!layout)return;
    XL6_IrType* type=(XL6_IrType*)xl6IrTypeGet(&m->types,id);XL6_IrType saved=*type;++enums;
    type->nominalSymbol=0;refused(m,target);*type=saved;
    type->integerTarget=XL6_IR_INTEGER_TARGET_SIGNED_32;refused(m,target);*type=saved;
    size_t count=layout->variantCount;--layout->variantCount;refused(m,target);layout->variantCount=count;
    XL6_IrEnumVariant* item=&m->enumVariants[layout->firstVariant];char* name=item->name;
    item->name="not_the_registered_flag";refused(m,target);item->name=name;
    XL6_IrTypeId* slot=target->returnType==id?&target->returnType:NULL;
    for(size_t i=0;!slot&&i<target->parameterCount;++i)if(target->parameterTypes[i]==id)slot=&target->parameterTypes[i];
    assert(slot);*slot=integer;refused(m,target);*slot=id;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));XL6_IrModule* m=&unit.ir;
    assert(xl6IrVerifyModule(m,NULL));XL6_IrTypeId integer=0,info=0,control=0,errorInfo=0;
    for(size_t i=0;i<m->types.count;++i){const XL6_IrType* type=&m->types.items[i];
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_I64&&type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;}
    for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* target=&m->callTargets[i];if(!target->cName)continue;
        if(!strcmp(target->cName,"xl6NetDatagramInfoCreateV1"))info=target->returnType;
        if(!strcmp(target->cName,"xl6NetDatagramControlCreateV1"))control=target->returnType;
        if(!strcmp(target->cName,"xl6NetDatagramErrorInfoCreateV1"))errorInfo=target->returnType;}
    assert(integer&&info&&control&&errorInfo&&info!=control&&info!=errorInfo);
    for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* target=&m->callTargets[i];if(!selected(target->cName))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(m,target));XL6_IrCallTarget saved=*target;++targets;
        target->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;refused(m,target);*target=saved;
        target->effectAbi=saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE;refused(m,target);*target=saved;
        target->executionBoundaryAbi=saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        refused(m,target);*target=saved;
        if(saved.errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_NONE){target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;refused(m,target);*target=saved;}
        enumContract(m,target,target->returnType,integer);
        for(size_t j=0;j<target->parameterCount;++j){XL6_IrTypeId id=target->parameterTypes[j];enumContract(m,target,id,integer);
            if(id==info||id==control||id==errorInfo){target->parameterTypes[j]=id==info?control:info;refused(m,target);target->parameterTypes[j]=id;++owners;}}
        const XL6_IrType* result=xl6IrTypeGet(&m->types,target->returnType);
        if(result&&result->nativeProductAbi==XL6_IR_NATIVE_PRODUCT_I64U64_V1){++products;
            XL6_IrType* tuple=(XL6_IrType*)result;XL6_IrTypeId status=tuple->params[0];tuple->params[0]=integer;refused(m,target);tuple->params[0]=status;}
    }
    #if defined(TEST_DATAGRAM_ERROR_ONLY)
    assert(targets==21&&products==1&&owners==15&&enums==4&&refusals>=110&&xl6IrVerifyModule(m,NULL));
    #else
    assert(targets==55&&products==7&&owners==39&&enums==10&&refusals>=290&&xl6IrVerifyModule(m,NULL));
    #endif
    xl6CompileUnitFree(&unit);
    printf("Datagram XLIR: %zu targets, %zu products, %zu exact owners, %zu enum positions, %zu corrupt contracts refused without C/header\n",targets,products,owners,enums,refusals);return 0;
}
