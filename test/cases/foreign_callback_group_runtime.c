#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_native_callback.h"
#include "src/api/xl_foreign_error.h"
#include "src/api/xl_api_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned pins,acquisitions,rejectAt,calls,edges;
static bool closed,closeOnAcquire;
static unsigned groupSymbols;
static int visit_symbol(void* unused,const char* name,const void* address)
{
    (void)unused;
    if(!strcmp(name,"xl6NativeCallbackGroupRuntimeV1Get")){
        assert(address==(const void*)xl6NativeCallbackGroupRuntimeV1Get);++groupSymbols;
    }
    return 1;
}
static int acquire(void* unused)
{
    (void)unused;++acquisitions;
    if(closed || (rejectAt && acquisitions==rejectAt))return 0;
    ++pins;if(closeOnAcquire){closed=true;closeOnAcquire=false;}return 1;
}
static void release_code(void* unused){(void)unused;assert(pins);--pins;}
static void entry(void){++calls;}
static bool visit(xrtownershipref ref,void* unused)
{(void)unused;assert(ref.Data && ref.Ops);++edges;return true;}
static void clean(xl6_error_result result){assert(result.ok && !result.error && !xrtGetError());}
static void thunk(void* user)
{
    const XL6_NativeCallbackGroupV1* group=user;
    XL6_NativeCallback* record=group && group->Count>1 && group->Members ? group->Members[1]:NULL;
    XL6_NativeCallbackScopeV1 scope={0};
    assert(xl6NativeCallbackRuntimeV1Get()->Enter(record,17,&scope,NULL,0));
    scope.closure.closure->invoke();
    clean(xl6NativeCallbackRuntimeV1Get()->Leave(&scope,(xl6_error_result){true,NULL}));
}
static const xrtownershipadapterv1* adapter_for(xrtownershipref ref)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;
    assert(xl6OwnershipDomainNativeCallbackV1Get()->Resolve(ref,NULL,&adapter,&preparation,&completion));
    assert(adapter && !preparation && !completion);return adapter;
}
int main(void)
{
    assert(xlApiVisitDefaultRuntimeSymbols(visit_symbol,NULL) && groupSymbols==1);
    assert(xrtMemDebugEnable(true));XL6_CodeOwner owner={sizeof(owner),&pins,acquire,release_code};
    const XL6_ClosureRuntimeV1* closure=xl6ClosureRuntimeV1Get();
    const XL6_NativeCallbackGroupRuntimeV1* api=xl6NativeCallbackGroupRuntimeV1Get();
    XL6_Closure* function=xl6ClosureRuntimeV2Get()->Create(&owner,entry,NULL,NULL,NULL,NULL,NULL,0);assert(function);
    unsigned baselinePins=pins;assert(baselinePins);
    const XL6_NativeCallbackRuntimeV1* single=xl6NativeCallbackRuntimeV1Get();
    XL6_NativeCallback* ordinary=single->Create(&owner,function,NULL,17);assert(ordinary);
    XL6_NativeCallbackScopeV1 ordinaryScope={0};assert(single->HoldNative(ordinary));
    assert(single->Enter(ordinary,17,&ordinaryScope,NULL,0));ordinaryScope.closure.closure->invoke();
    clean(single->Leave(&ordinaryScope,(xl6_error_result){true,NULL}));
    assert(single->DropNative(ordinary) && single->Release(ordinary) && pins==baselinePins);
    XL6_Closure* members[]={function,function,NULL};const uint64_t signatures[]={13,17,0};
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    XL6_NativeCallbackGroupV1* group=api->Create(&owner,members,NULL,signatures,3);assert(group);
    xrtMemDebugSnapshot(&after);
    printf("GROUP_COLD alloc=%llu bytes=%llu\n",(unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.LiveBytes-before.LiveBytes));
    assert(group->Count==3 && group->Members[0] && group->Members[1] && !group->Members[2]);
    assert(group->Members[0]!=group->Members[1] && pins>baselinePins);
    xrtownershipref ref=api->Ownership(group);size_t refs=0;
    assert(ref.Ops->Count(ref.Data,&refs) && refs==1);
    edges=0;assert(ref.Ops->Trace(ref.Data,visit,NULL) && edges==3);
    const xrtownershipadapterv1* adapter=adapter_for(ref);
    assert(adapter->Hold(ref.Data));int token;
    assert(adapter->Claim(ref.Data,&token));assert(!api->Retain(group));xrtClearError();
    adapter->Restore(ref.Data,&token);adapter->Drop(ref.Data);
    assert(api->Retain(group)==group && ref.Ops->Count(ref.Data,&refs) && refs==2);assert(api->Release(group));
    XL6_NativeCallbackScopeV1 wrong={0};assert(!xl6NativeCallbackRuntimeV1Get()->Enter(group->Members[1],99,&wrong,NULL,0));xrtClearError();
    assert(api->HoldNative(group));thunk(group);assert(api->DropNative(group));
    xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(0));
    for(unsigned i=0;i<1000;++i){assert(api->HoldNative(group));thunk(group);assert(api->DropNative(group));}
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    assert(calls==1002);assert(api->Release(group) && pins==baselinePins);
    acquisitions=0;closeOnAcquire=true;
    group=api->Create(&owner,members,NULL,signatures,3);assert(group && closed && acquisitions==1);
    assert(!single->Create(&owner,function,NULL,17));xrtClearError();
    assert(api->HoldNative(group));thunk(group);assert(api->DropNative(group));
    assert(api->Release(group) && pins==baselinePins);closed=false;
    group=api->Create(&owner,members,NULL,signatures,3);assert(group);ref=api->Ownership(group);adapter=adapter_for(ref);
    assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Clear(ref.Data,&token);
    assert(!group->Count && !group->Members && !api->Retain(group));xrtClearError();
    assert(adapter->Finish(ref.Data,&token));adapter->Drop(ref.Data);assert(api->Release(group) && pins==baselinePins);
    XL6_Closure* absent[]={NULL,NULL};uint64_t emptySignatures[]={0,0};
    group=api->Create(&owner,absent,NULL,emptySignatures,2);assert(group && group->Count==2);
    assert(!group->Members[0] && !group->Members[1]);edges=0;ref=api->Ownership(group);
    assert(ref.Ops->Trace(ref.Data,visit,NULL) && edges==1);assert(api->Release(group) && pins==baselinePins);
    assert(!api->Create(&owner,members,NULL,signatures,0));xrtClearError();
    assert(!api->Create(&owner,members,NULL,signatures,65));xrtClearError();
    assert(!api->Create(&owner,NULL,NULL,signatures,3));xrtClearError();
    assert(!api->Create(&owner,members,NULL,NULL,3));xrtClearError();
    uint64_t bad[]={0,17,0};assert(!api->Create(&owner,members,NULL,bad,3));xrtClearError();
    XL6_Closure* legacy=closure->Create(&owner,entry,NULL,NULL,NULL,NULL,NULL,0,NULL);assert(legacy);
    XL6_Closure* old[]={legacy};assert(!api->Create(&owner,old,NULL,signatures,1));xrtClearError();
    closure->Release(legacy);assert(pins==baselinePins);
    acquisitions=0;rejectAt=1;assert(!api->Create(&owner,members,NULL,signatures,3));
    rejectAt=0;xrtClearError();assert(pins==baselinePins);
    acquisitions=0;group=api->Create(&owner,members,NULL,signatures,3);assert(group && acquisitions==1);
    assert(api->Release(group) && pins==baselinePins);
    unsigned failures=0,successes=0;xrtMemDebugSnapshot(&before);
    for(int n=0;n<20;++n){
        assert(xrtMemDebugFailAfter(n));group=api->Create(&owner,members,NULL,signatures,3);xrtMemDebugFailClear();
        if(group){++successes;assert(api->Release(group));}else{++failures;xrtClearError();}
        assert(pins==baselinePins);
    }
    xrtMemDebugSnapshot(&after);assert(failures && successes);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(api->Release(NULL) && api->DropNative(NULL));assert(!api->Retain(NULL));xrtClearError();
    closure->Release(function);assert(!pins && !xrtGetError());
    printf("GROUP_RUNTIME_OK oom=%u/%u calls=%u hotAlloc=0\n",failures,successes,calls);return 0;
}
