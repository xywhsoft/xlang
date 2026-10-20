/* Local OS metadata only; no network connections or external name resolution. */
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib_data.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Link-only subcall doubles exercise OS size-change outcomes deterministically.
 * Public JIT/module/AOT fixtures still use the real native providers. */
extern size_t __real_xrtNetHostName(char*,size_t);
extern size_t __real_xrtNetLocalHardware(void*,size_t);
static unsigned hostMode,hardwareMode,hostCalls,hardwareCalls;
size_t __wrap_xrtNetHostName(char* output,size_t capacity)
{
    if(!hostMode)return __real_xrtNetHostName(output,capacity);
    ++hostCalls;
    if(hostMode==2||capacity<=200){xrtSetErrorKind(XERR_RANGE);return hostMode==2?capacity:200;}
    memset(output,'a',200);output[200]=0;return 200;
}
size_t __wrap_xrtNetLocalHardware(void* output,size_t capacity)
{
    if(!hardwareMode)return __real_xrtNetLocalHardware(output,capacity);
    ++hardwareCalls;
    if(hardwareMode==2||capacity<100){xrtSetErrorKind(XERR_RANGE);return hardwareMode==2?capacity+1:100;}
    memset(output,255,100);return 100;
}
#define XL6_STDLIB_FUNC_INDEX_CAPACITY 4096u
#include "src/stdlib/xl6_stdlib_func_index.inc"
static void registration_contracts(void)
{
    assert(xl6StdlibFuncCount==XL6_STDLIB_FUNC_COUNT);
    for(size_t n=0;n<xl6StdlibFuncCount;++n){
        uint64_t hash=UINT64_C(1469598103934665603);const XL6_StdlibFunc* expected=&xl6StdlibFuncs[n];
        for(const unsigned char* p=(const unsigned char*)expected->ns;*p;++p)hash=(hash^*p)*UINT64_C(1099511628211);
        hash=(hash^255u)*UINT64_C(1099511628211);
        for(const unsigned char* p=(const unsigned char*)expected->name;*p;++p)hash=(hash^*p)*UINT64_C(1099511628211);
        size_t at=(size_t)hash&(XL6_STDLIB_FUNC_INDEX_CAPACITY-1);bool found=false;
        for(size_t probe=0;probe<XL6_STDLIB_FUNC_INDEX_CAPACITY&&xl6StdlibFuncIndex[at];++probe){
            size_t index=xl6StdlibFuncIndex[at]-1;assert(index<xl6StdlibFuncCount);
            const XL6_StdlibFunc* actual=&xl6StdlibFuncs[index];
            /* A namespace key can own several intentional overload rows.
             * Every row must occur in its complete linear-probe chain. */
            if(index==n){assert(!strcmp(actual->ns,expected->ns)&&!strcmp(actual->name,expected->name));found=true;break;}
            at=(at+1)&(XL6_STDLIB_FUNC_INDEX_CAPACITY-1);
        }
        assert(found);
    }
    printf("stdlib registration: %zu exact indexed declarations\n",xl6StdlibFuncCount);
}
static size_t refusals;
static void reject(XL6_IrModule* module,XL6_IrCallTarget* target)
{
    if(target)assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;XL6_DiagList diags;size_t size=SIZE_MAX;
        xl6DiagInit(&diags);assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_INTERFACE",&diags):xl6IrCgenEmitC(&out,module,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(size==0);
        xrtFree(text);xl6OutputDiscard(&out);xl6DiagFree(&diags);
    }
    ++refusals;
}
static void contracts(const char* path,unsigned mode)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags)&&xl6IrVerifyModule(&unit.ir,NULL));
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&unit.ir,XL6_ARTIFACT_ENTRY_EXECUTABLE_V1,&plan,NULL));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NET_INTERFACES)==(mode!=2));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NET_INTERFACE_QUERY)==(mode!=0));
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NET_INTERFACE_LOCAL)==(mode==1));
    assert(!xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NETWORK_SERVICE));
    for(size_t n=0;n<plan.nativeModuleCount;++n)
        assert(!strstr(plan.nativeModules[n],"DNS")&&!strstr(plan.nativeModules[n],"ENGINE")&&
            !strstr(plan.nativeModules[n],"RESOLVER")&&!strstr(plan.nativeModules[n],"TLS"));
    unsigned checked=0;
    for(size_t n=0;n<unit.ir.callTargetCount;++n){
        XL6_IrCallTarget* target=&unit.ir.callTargets[n];
        if(!target->cName || (strncmp(target->cName,"xl6NetInterface",15)&&strncmp(target->cName,"xl6NetLocal",11)&&strcmp(target->cName,"xl6NetHostNameV1")))continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));XL6_IrCallTarget saved=*target;
        target->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;
        reject(&unit.ir,target);*target=saved;
        target->effectAbi=saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE;
        reject(&unit.ir,target);*target=saved;
        target->executionBoundaryAbi=saved.executionBoundaryAbi==XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING?
            XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK:XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING;
        reject(&unit.ir,target);*target=saved;
        target->nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;reject(&unit.ir,target);*target=saved;
        if(saved.resultPhysicalDestroySymbol){
            target->resultPhysicalDestroySymbol=0;reject(&unit.ir,target);*target=saved;
            target->resultPhysicalFieldDeclaration=XL6_IR_NODE_INVALID;reject(&unit.ir,target);*target=saved;
        }
        if(saved.nativeHandlePolicyCName){target->nativeHandlePolicyCName="xl6NetAddressHandlePolicyV1Get";reject(&unit.ir,NULL);*target=saved;}
        for(size_t at=0;at<target->parameterCount;++at){
            XL6_IrNodeId field=target->parameterPhysicalFieldDeclarations[at];if(!field)continue;
            target->parameterPhysicalFieldDeclarations[at]=XL6_IR_NODE_INVALID;reject(&unit.ir,target);target->parameterPhysicalFieldDeclarations[at]=field;
        }
        ++checked;
    }
    assert(checked>=(mode==2?1u:20u)&&xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool forbiddenVisit(xrtownershipref ref,void* context)
{(void)ref;(void)context;assert(false);return false;}
static void retry_contracts(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);hostMode=1;hostCalls=0;
    xstrview name=xl6NetHostNameV1();assert(name.Size==200&&name.Data&&name.Data[0]=='a'&&hostCalls==2&&!xrtGetError());
    xrtFree((void*)name.Data);hostMode=0;balanced(&before);
    xrtMemDebugSnapshot(&before);hostMode=2;hostCalls=0;name=xl6NetHostNameV1();
    assert(!name.Data&&hostCalls==8&&xrtErrorKind(xrtGetError())==XERR_STATE);hostMode=0;balanced(&before);
    xrtMemDebugSnapshot(&before);hardwareMode=1;hardwareCalls=0;
    xvalue* value=xl6NetLocalHardwareV1();xbytesview bytes;
    assert(value&&xrtValueGetBytes(value,&bytes)&&bytes.Size==100&&bytes.Data[99]==255&&hardwareCalls==2&&!xrtGetError());
    xrtValueRelease(value);hardwareMode=0;balanced(&before);
    xrtMemDebugSnapshot(&before);hardwareMode=2;hardwareCalls=0;value=xl6NetLocalHardwareV1();
    assert(!value&&hardwareCalls==8&&xrtErrorKind(xrtGetError())==XERR_STATE);hardwareMode=0;balanced(&before);
    puts("interface adapters: real-read growth retry, bounded refusal, no partial text/bytes, balanced storage");
}
static void semantic_contracts(void)
{
    static const struct {const char* source;const char* error;} cases[]={
        {"class Probe\nnative point handle\nend\npoint raw = null\nauto result = Probe(raw)\n",NULL},
        {"class Probe\nprivate native point handle\nend\npoint raw = null\nauto result = Probe(raw)\n","private class member is not accessible"},
        {"class Probe\nprotected native point handle\nend\npoint raw = null\nauto result = Probe(raw)\n","protected class member is not accessible"},
        {"class Probe\nprivate native point handle\nstatic func wrap(point raw) as Probe\nreturn Probe(raw)\nend\nend\npoint raw = null\nauto result = Probe.wrap(raw)\n",NULL},
        {"class Base\nprotected native point handle\nend\nclass Child extends Base\nstatic func wrap(point raw) as Child\nreturn Child(raw)\nend\nend\npoint raw = null\nauto result = Child.wrap(raw)\n",NULL},
        {"class Base\nprivate native point handle\nend\nclass Child extends Base\nstatic func wrap(point raw) as Child\nreturn Child(raw)\nend\nend\n","private class member is not accessible"},
        {"class Base\nprotected native point handle\nend\nclass Child extends Base\nend\npoint raw = null\nauto result = Child(raw)\n","protected class member is not accessible"},
        {"class Base\nprivate native point handle\nend\nclass Child extends Base\nint handle\nend\npoint raw = null\nauto result = Child(raw)\n","private class member is not accessible"},
        {"class Probe\nprivate native point handle\nextern \"explicit_adopt_contract\" create(point raw)\nend\npoint raw = null\nauto result = Probe(raw)\n",NULL},
        {"func answer() as int\nreturn 42\nend\nclass Host\nfunc apply<T>(func() as T work) as T\nreturn work()\nend\nfunc apply(func() work)\nwork()\nend\nend\nauto result = Host().apply(answer)\n",NULL},
        {"func answer() as int\nreturn 42\nend\nfunc apply(func() work) as int\nwork()\nreturn 0\nend\nfunc apply(func() as int work) as int\nreturn work()\nend\nauto result = apply(answer)\n",NULL},
        {"func answer() as int\nreturn 42\nend\nfunc() work = answer\n","找不到和 func 目标签名匹配的具名函数"},
        {"func answer(int value) as int\nreturn value\nend\nfunc apply(func() work) as int\nwork()\nreturn 0\nend\nfunc apply(func() as int work) as int\nreturn work()\nend\nauto result = apply(answer)\n",""}
    };
    for(size_t n=0;n<sizeof(cases)/sizeof(cases[0]);++n){
        XL6_ParseResult parsed;xl6ParseResultInit(&parsed);assert(xl6ParseSource(cases[n].source,"native_privacy.xl",&parsed));
        bool ok=xl6SemCheckModule(parsed.root,&parsed.diags,"native_privacy")!=0;
        assert(ok==(cases[n].error==NULL));
        if(!ok){
            bool found=false;
            for(size_t at=0;at<parsed.diags.count;++at){
                if(strstr(parsed.diags.items[at].message,cases[n].error))found=true;
            }
            assert(found);
        }
        xl6ParseResultFree(&parsed);
    }
    puts("semantic contracts: 13 privacy/inheritance/explicit-constructor/quiet-overload-probe/actual-mismatch cases");
}
static void native(void)
{
    assert(xrtMemDebugEnable(true));retry_contracts();xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xnetinterfaceaddress addresses[2]={0};
    assert(xrtNetAddrLoopback(&addresses[0].Address,XNET_FAMILY_IPV4,0));addresses[0].PrefixLength=8;
    assert(xrtNetAddrLoopback(&addresses[1].Address,XNET_FAMILY_IPV6,0));addresses[1].PrefixLength=255;
    char name[3]={'a',0,'b'},display[]="网卡";uint8 hardware[]={0,1,0,255};
    xnetinterface source={17,18,XNET_INTERFACE_UP|XNET_INTERFACE_LOOPBACK,1500,
        {name,sizeof(name)},{display,sizeof(display)-1},{hardware,sizeof(hardware)},addresses,2};
    xnetinterfacelist list={&source,1};void* value=xl6NetInterfacesGetV1(&list,0);assert(value);
    memset(name,'x',sizeof(name));memset(display,'x',sizeof(display)-1);memset(hardware,9,sizeof(hardware));
    xstrview text=xl6NetInterfaceNameV1(value);assert(text.Size==3&&!memcmp(text.Data,"a\0b",3));xrtFree((void*)text.Data);
    text=xl6NetInterfaceDisplayNameV1(value);assert(text.Size==6&&!memcmp(text.Data,"网卡",6));xrtFree((void*)text.Data);
    xvalue* data=xl6NetInterfaceHardwareV1(value);xbytesview bytes;assert(data&&xrtValueGetBytes(data,&bytes));
    assert(bytes.Size==4&&bytes.Data[0]==0&&bytes.Data[3]==255);xrtValueRelease(data);
    xrtSetErrorKind(XERR_VALUE);const xerror* first=xrtGetError();
    assert(xl6NetInterfaceIPv4IndexV1(value)==17&&xl6NetInterfaceIPv6IndexV1(value)==18&&xl6NetInterfaceMtuV1(value)==1500);
    assert(xl6NetInterfaceFlagsV1(value)==5&&xl6NetInterfaceHasFlagV1(value,5)&&!xl6NetInterfaceHasFlagV1(value,-1));
    assert(xl6NetInterfaceIsUpV1(value)&&!xl6NetInterfaceIsRunningV1(value)&&xl6NetInterfaceIsLoopbackV1(value));
    assert(!xl6NetInterfaceSupportsBroadcastV1(value)&&!xl6NetInterfaceIsPointToPointV1(value)&&!xl6NetInterfaceSupportsMulticastV1(value));
    assert(xl6NetInterfaceAddressCountV1(value)==2&&xrtGetError()==first);xrtClearError();
    void* entry=xl6NetInterfaceAddressV1(value,1);assert(entry);xl6NetInterfaceDestroyV1(value);
    assert(xl6NetInterfaceAddressFamilyV1(entry)==6&&!xl6NetInterfaceAddressPrefixKnownV1(entry));
    void* address=xl6NetInterfaceAddressValueV1(entry);assert(address);xl6NetInterfaceAddressDestroyV1(entry);
    assert(xl6NetAddressIsLoopbackV1(address));xl6NetAddressDestroyV1(address);balanced(&before);
    const XL6_NativeHandlePolicyV1* policy=xl6NetInterfaceHandlePolicyV1Get();
    assert(policy->Trace==xl6NativeHandleTraceLeaf&&xl6NetInterfacesHandlePolicyV1Get()->Trace==xl6NativeHandleTraceLeaf&&
        xl6NetInterfaceAddressHandlePolicyV1Get()->Trace==xl6NativeHandleTraceLeaf);
    unsigned prefixes=0;
    for(size_t point=0;point<40;++point){
        xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
        value=xl6NetInterfaceCopyV1(&source);text=(xstrview){0};entry=NULL;address=NULL;data=NULL;xvalue* boxed=NULL;
        if(value){text=xl6NetInterfaceNameV1(value);if(text.Data){entry=xl6NetInterfaceAddressV1(value,0);
            if(entry){address=xl6NetInterfaceAddressValueV1(entry);if(address){data=xl6NetInterfaceHardwareV1(value);
                if(data)boxed=xl6NativeHandleTakeV1(&value,policy);}}}}
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();first=xrtGetError();if(hit)assert(first);
        if(!hit)assert(boxed&&policy->Trace(boxed,forbiddenVisit,NULL)&&text.Data&&entry&&address&&data);
        xrtValueRelease(boxed);xl6NetInterfaceDestroyV1(value);xrtFree((void*)text.Data);
        xl6NetInterfaceAddressDestroyV1(entry);xl6NetAddressDestroyV1(address);xrtValueRelease(data);
        if(hit)assert(xrtGetError()==first);
        balanced(&before);
        if(!hit)break;
        ++prefixes;
    }
    assert(prefixes>=6&&prefixes<40);
    source.Name.Size=SIZE_MAX;assert(!xl6NetInterfaceCopyV1(&source)&&xrtErrorKind(xrtGetError())==XERR_RANGE);source.Name.Size=3;
    source.AddressCount=SIZE_MAX;assert(!xl6NetInterfaceCopyV1(&source)&&xrtErrorKind(xrtGetError())==XERR_RANGE);source.AddressCount=2;
    addresses[0].PrefixLength=33;assert(!xl6NetInterfaceCopyV1(&source)&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);addresses[0].PrefixLength=8;
    xrtClearError();name[0]=(char)255;value=xl6NetInterfaceCopyV1(&source);assert(value);
    text=xl6NetInterfaceNameV1(value);assert(!text.Data&&xrtGetError());xl6NetInterfaceDestroyV1(value);name[0]='x';
    xrtClearError();unsigned osPrefixes=0;
    for(size_t point=0;point<128;++point){
        xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));value=xl6NetInterfacesCreateV1();
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();if(!hit)assert(value);
        xl6NetInterfacesDestroyV1(value);balanced(&before);if(!hit)break;++osPrefixes;
    }
    assert(osPrefixes>0&&osPrefixes<128);
    printf("interfaces native: %u detached-copy prefixes, %u OS snapshot prefixes, leaf policies, exact balanced storage\n",prefixes,osPrefixes);
}
int main(int argc,char** argv)
{
    assert(argc==4);registration_contracts();semantic_contracts();for(unsigned n=0;n<3;++n)contracts(argv[n+1],n);native();
    printf("interfaces XLIR: %zu corrupt contracts refused before C/header output\n",refusals);return 0;
}
