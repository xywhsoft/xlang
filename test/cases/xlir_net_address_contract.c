/* Offline address ABI, immutable POD ownership and fail-closed XLIR checks. */
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals;
static void reject(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    if(target)assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;XL6_DiagList diags;size_t size=SIZE_MAX;
        xl6DiagInit(&diags);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_ADDRESS",&diags):xl6IrCgenEmitC(&out,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(size==0);
        xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&diags);
    }
    ++refusals;
}
static void contracts(const char* path,bool dns)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(xl6IrVerifyModule(&unit.ir,NULL));
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NET_ADDRESS));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NET_RESOLVE_ADDRESS)==dns);
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NETWORK_SERVICE));
    for(size_t n=0;n<plan.nativeModuleCount;++n){
        assert(!strstr(plan.nativeModules[n],"ENGINE")&&!strstr(plan.nativeModules[n],"RESOLVER")&&
            !strstr(plan.nativeModules[n],"TLS")&&!strstr(plan.nativeModules[n],"NET_INTERFACE"));
        if(!dns)assert(!strstr(plan.nativeModules[n],"DNS"));
    }
    unsigned checked=0;
    for(size_t n=0;n<unit.ir.callTargetCount;++n){
        XL6_IrCallTarget* target=&unit.ir.callTargets[n];
        if(!target->cName || (strncmp(target->cName,"xl6NetAddress",13)&&strcmp(target->cName,"xl6NetResolveAddressV1")))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));
        XL6_IrCallTarget saved=*target;
        target->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;
        reject(&unit.ir,target);*target=saved;
        target->effectAbi=saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE;
        reject(&unit.ir,target);*target=saved;
        target->nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;
        reject(&unit.ir,target);*target=saved;
        target->executionBoundaryAbi=saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?
            XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        reject(&unit.ir,target);*target=saved;
        if(saved.resultPhysicalDestroySymbol){
            target->resultPhysicalDestroySymbol=0;reject(&unit.ir,target);*target=saved;
            target->resultPhysicalFieldDeclaration=XL6_IR_NODE_INVALID;reject(&unit.ir,target);*target=saved;
        }
        if(saved.nativeHandlePolicyCName){
            target->nativeHandlePolicyCName="xl6DateTimeHandlePolicyV1Get";
            /* Policy identity is independently checked by the complete IR
             * verifier, not by the narrower direct-call shape predicate. */
            reject(&unit.ir,NULL);*target=saved;
        }
        for(size_t at=0;at<target->parameterCount;++at){
            if(target->parameterPhysicalFieldDeclarations[at]==XL6_IR_NODE_INVALID)continue;
            XL6_IrNodeId field=target->parameterPhysicalFieldDeclarations[at];
            target->parameterPhysicalFieldDeclarations[at]=XL6_IR_NODE_INVALID;
            reject(&unit.ir,target);target->parameterPhysicalFieldDeclarations[at]=field;
        }
        ++checked;
    }
    assert(checked>=(dns?3u:20u)&&xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool forbiddenVisit(xrtownershipref ref,void* context)
{(void)ref;(void)context;assert(false);return false;}
static void native(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const char ip[]={'1','2','7','.','0','.','0','.','1'};
    void* a=xl6NetAddressParseV1((xstrview){ip,sizeof(ip)},123);assert(a);
    xrtSetErrorKind(XERR_VALUE);const xerror* saved=xrtGetError();assert(saved);
    assert(xl6NetAddressFamilyV1(a)==4&&xl6NetAddressPortV1(a)==123&&xl6NetAddressScopeV1(a)==0);
    assert(xl6NetAddressIsLoopbackV1(a)&&!xl6NetAddressIsMappedV1(a)&&!xl6NetAddressIsPrivateV1(a));
    assert(!xl6NetAddressIsUnspecifiedV1(a)&&!xl6NetAddressIsMulticastV1(a)&&!xl6NetAddressIsLinkLocalV1(a));
    assert(xl6NetAddressEqualV1(a,a)&&xl6NetAddressSameIPV1(a,a)&&xl6NetAddressCompareV1(a,a)==0);
    assert(xrtGetError()==saved);xrtClearError();
    xstrview text=xl6NetAddressEndpointTextV1(a);assert(text.Size==13&&!memcmp(text.Data,"127.0.0.1:123",13));
    xrtFree((void*)text.Data);
    xvalue* bytes=xl6NetAddressBytesV1(a);xbytesview view;assert(bytes&&xrtValueGetBytes(bytes,&view));
    assert(view.Size==4&&view.Data[0]==127&&view.Data[3]==1);xrtValueRelease(bytes);
    void* b=xl6NetAddressWithPortV1(a,124);assert(b&&b!=a&&xl6NetAddressPortV1(a)==123);
    assert(xl6NetAddressSameIPV1(a,b)&&!xl6NetAddressEqualV1(a,b)&&xl6NetAddressCompareV1(a,b)<0);
    xl6NetAddressDestroyV1(b);
    const XL6_NativeHandlePolicyV1* policy=xl6NetAddressHandlePolicyV1Get();
    assert(policy->abiTag==XL6_NATIVE_HANDLE_POLICY_V1_TAG&&policy->Trace==xl6NativeHandleTraceLeaf);
    xvalue* handle=xl6NativeHandleTakeV1(&a,policy);assert(handle&&!a);
    assert(policy->Trace(handle,forbiddenVisit,NULL));xrtValueRelease(handle);balanced(&before);
    unsigned prefixes=0;
    for(size_t point=0;point<40;++point){
        xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
        void* address=xl6NetAddressParseV1((xstrview){ip,sizeof(ip)},80);
        xstrview printed={0};xvalue* data=NULL;void* copy=NULL;
        if(address){printed=xl6NetAddressTextV1(address);if(printed.Data){data=xl6NetAddressBytesV1(address);
            if(data)copy=xl6NetAddressWithPortV1(address,81);}}
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();const xerror* failure=xrtGetError();
        if(hit)assert(failure);
        if(!hit)assert(address&&printed.Data&&data&&copy);
        xl6NetAddressDestroyV1(copy);xrtValueRelease(data);xrtFree((void*)printed.Data);xl6NetAddressDestroyV1(address);
        if(hit)assert(xrtGetError()==failure);
        balanced(&before);if(!hit)break;++prefixes;
    }
    assert(prefixes>=4&&prefixes<40);
    xrtMemDebugSnapshot(&before);a=xl6NetAddressDefaultV1();assert(a);assert(xrtMemDebugFailAfter(0));
    handle=xl6NativeHandleTakeV1(&a,policy);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    assert(hit&&!handle&&!a);balanced(&before);
    printf("address native: %u allocation prefixes, consume-on-bind-failure, leaf trace and balanced storage\n",prefixes);
}
int main(int argc,char** argv)
{
    assert(argc==3);contracts(argv[1],false);contracts(argv[2],true);native();
    printf("address XLIR: %zu corrupt contracts refused before C/header output\n",refusals);return 0;
}
