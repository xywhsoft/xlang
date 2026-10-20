/* Pure configuration/lifecycle/OOM. No Engine or Socket creation, DNS or I/O. */
#include "src/runtime/xl6_tcp_options_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef TEST_ROOT
#error TEST_ROOT is required
#endif
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void errorEnd(xl6_error_scope* scope,xerrkind kind)
{xl6_error_result result=xl6ErrorScopeEnd(scope);assert(!result.ok&&xrtErrorKind(result.error)==kind);xrtErrorFree(result.error);assert(!xrtGetError());}
static void handle(void** raw,const XL6_NativeHandlePolicyV1* policy)
{
    xvalue* value=xl6NativeHandleTakeV1(raw,policy);assert(value&&!*raw);
    xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count;
    assert(xrtOwnershipFreezeTryBegin(&freeze)&&ref.Ops->Count(ref.Data,&count)&&count==1);
    const xrtownershipadapterv1* adapter=xrtValueHandleOwnershipAdapterV1(ref,policy->Ops,policy->Trace);assert(adapter);
    int token=0;assert(adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xrtValueRelease(value);
}
static void stream(void)
{
    void* original=xl6TcpStreamOptionsCreateV1();assert(original);
    assert(xl6TcpStreamOptionsReadSizeV1(original)==2048&&xl6TcpStreamOptionsReadLimitV1(original)==1048576);
    assert(xl6TcpStreamOptionsWriteHighWaterV1(original)==262144&&xl6TcpStreamOptionsWriteLowWaterV1(original)==65536&&xl6TcpStreamOptionsWriteLimitV1(original)==1048576);
    assert(xl6TcpStreamOptionsConnectTimeoutV1(original)==30000&&xl6TcpStreamOptionsReadModeV1(original)==0);
    assert(xl6TcpStreamOptionsNoDelayV1(original)&&!xl6TcpStreamOptionsKeepAliveV1(original));
    void* changed=xl6TcpStreamOptionsWithReadBufferV1(original,3,13);assert(changed&&((xnetstreamconfig*)changed)->ReadSize==3);
    xl6TcpStreamOptionsDestroyV1(changed);changed=xl6TcpStreamOptionsWithWriteWatermarksV1(original,11,2,31);assert(changed&&((xnetstreamconfig*)changed)->WriteLowWater==2);
    xl6TcpStreamOptionsDestroyV1(changed);changed=xl6TcpStreamOptionsWithConnectTimeoutV1(original,17);assert(changed&&((xnetstreamconfig*)changed)->ConnectTimeout==17000);
    xl6TcpStreamOptionsDestroyV1(changed);changed=xl6TcpStreamOptionsWithConnectTimeoutV1(original,-1);assert(changed&&!((xnetstreamconfig*)changed)->ConnectTimeout);
    xl6TcpStreamOptionsDestroyV1(changed);changed=xl6TcpStreamOptionsWithReadModeV1(original,2);assert(changed&&xl6TcpStreamOptionsReadModeV1(changed)==2);
    xl6TcpStreamOptionsDestroyV1(changed);changed=xl6TcpStreamOptionsWithNoDelayV1(original,false);assert(changed&&!xl6TcpStreamOptionsNoDelayV1(changed));
    xl6TcpStreamOptionsDestroyV1(changed);changed=xl6TcpStreamOptionsWithKeepAliveV1(original,true);assert(changed&&xl6TcpStreamOptionsKeepAliveV1(changed));
    assert(xl6TcpStreamOptionsNoDelayV1(original)&&!xl6TcpStreamOptionsKeepAliveV1(original)&&xl6TcpStreamOptionsReadSizeV1(original)==2048);
    xl6TcpStreamOptionsDestroyV1(changed);
    xnetstreamconfig precise=*(xnetstreamconfig*)original;precise.ConnectTimeout=17001;changed=xl6TcpStreamOptionsCopyV1(&precise);
    assert(changed&&xl6TcpStreamOptionsConnectTimeoutV1(changed)==18&&((xnetstreamconfig*)changed)->ConnectTimeout==17001);xl6TcpStreamOptionsDestroyV1(changed);
    assert(xrtMemDebugFailAfter(0));assert(xl6TcpStreamOptionsReadSizeV1(original)==2048&&!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpStreamOptionsWithReadBufferV1(original,0,1));errorEnd(&error,XERR_ARGUMENT);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpStreamOptionsWithWriteWatermarksV1(original,11,12,20));errorEnd(&error,XERR_ARGUMENT);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpStreamOptionsWithConnectTimeoutV1(original,0));errorEnd(&error,XERR_RANGE);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpStreamOptionsWithConnectTimeoutV1(original,INT64_MAX));errorEnd(&error,XERR_RANGE);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpStreamOptionsWithReadModeV1(original,3));errorEnd(&error,XERR_RANGE);
    handle(&original,xl6TcpStreamOptionsHandlePolicyV1Get());
}
#if TEST_ROOT==1||TEST_ROOT==4
static void dial(void)
{
    void* original=xl6TcpDialOptionsCreateV1();assert(original&&xl6TcpDialOptionsFamilyV1(original)==0&&xl6TcpDialOptionsAffinityV1(original)==0);
    assert(xl6TcpDialOptionsTimeoutV1(original)==30000&&xl6TcpDialOptionsFallbackDelayV1(original)==250&&xl6TcpDialOptionsMaxAttemptsV1(original)==8);
    void* input=xl6TcpStreamOptionsCreateV1();void* changed=xl6TcpDialOptionsWithStreamV1(original,input);assert(changed);
    xl6TcpStreamOptionsDestroyV1(input);xl6TcpDialOptionsDestroyV1(original);original=changed;
    input=xl6TcpDialOptionsStreamV1(original);assert(input);xl6TcpDialOptionsDestroyV1(original);xl6TcpStreamOptionsDestroyV1(input);
    original=xl6TcpDialOptionsCreateV1();changed=xl6TcpDialOptionsWithFamilyV1(original,6);assert(changed&&xl6TcpDialOptionsFamilyV1(changed)==6);xl6TcpDialOptionsDestroyV1(changed);
    changed=xl6TcpDialOptionsWithAffinityV1(original,UINT64_MAX);assert(changed&&xl6TcpDialOptionsAffinityV1(changed)==UINT64_MAX);xl6TcpDialOptionsDestroyV1(changed);
    changed=xl6TcpDialOptionsWithTimeoutV1(original,19);assert(changed&&((xnetdialconfig*)changed)->Timeout==19000);xl6TcpDialOptionsDestroyV1(changed);
    changed=xl6TcpDialOptionsWithFallbackDelayV1(original,0);assert(changed&&!xl6TcpDialOptionsFallbackDelayV1(changed));xl6TcpDialOptionsDestroyV1(changed);
    changed=xl6TcpDialOptionsWithMaxAttemptsV1(original,64);assert(changed&&xl6TcpDialOptionsMaxAttemptsV1(changed)==64);xl6TcpDialOptionsDestroyV1(changed);
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpDialOptionsWithMaxAttemptsV1(original,65));errorEnd(&error,XERR_ARGUMENT);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpDialOptionsWithTimeoutV1(original,0));errorEnd(&error,XERR_RANGE);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpDialOptionsWithFallbackDelayV1(original,-1));errorEnd(&error,XERR_RANGE);
    handle(&original,xl6TcpDialOptionsHandlePolicyV1Get());
}
#endif
#if TEST_ROOT>=2
static void listen(void)
{
    void* original=xl6TcpListenOptionsCreateV1();assert(original&&xl6TcpListenOptionsAcceptConcurrencyV1(original)==16&&xl6TcpListenOptionsAcceptQueueLimitV1(original)==256);
    assert(xl6TcpListenOptionsBacklogV1(original)==256&&!xl6TcpListenOptionsAffinityV1(original)&&!xl6TcpListenOptionsDistributionV1(original));
    assert(!(xl6TcpListenOptionsExclusiveAddressV1(original)&&(xl6TcpListenOptionsReuseAddressV1(original)||xl6TcpListenOptionsReusePortV1(original))));
    xnetaddr address;assert(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV6,1234));
    void* changed=xl6TcpListenOptionsCreateAddressV1(&address);assert(changed&&((xnetlistenconfig*)changed)->Address.Port==1234);xl6TcpListenOptionsDestroyV1(changed);
    changed=xl6TcpListenOptionsWithAddressV1(original,&address);assert(changed);address.Port=99;
    assert(((xnetlistenconfig*)changed)->Address.Port==1234&&!((xnetlistenconfig*)original)->Address.Port);xl6TcpListenOptionsDestroyV1(changed);
    void* input=xl6TcpStreamOptionsCreateV1();changed=xl6TcpListenOptionsWithStreamV1(original,input);assert(changed);xl6TcpStreamOptionsDestroyV1(input);
    input=xl6TcpListenOptionsStreamV1(changed);assert(input);xl6TcpListenOptionsDestroyV1(changed);xl6TcpStreamOptionsDestroyV1(input);
    changed=xl6TcpListenOptionsWithAffinityV1(original,UINT64_MAX);assert(changed&&xl6TcpListenOptionsAffinityV1(changed)==UINT64_MAX);xl6TcpListenOptionsDestroyV1(changed);
    changed=xl6TcpListenOptionsWithAcceptQueueV1(original,4,13);assert(changed&&xl6TcpListenOptionsAcceptConcurrencyV1(changed)==4&&xl6TcpListenOptionsAcceptQueueLimitV1(changed)==13);xl6TcpListenOptionsDestroyV1(changed);
    changed=xl6TcpListenOptionsWithBacklogV1(original,32);assert(changed&&xl6TcpListenOptionsBacklogV1(changed)==32);xl6TcpListenOptionsDestroyV1(changed);
    changed=xl6TcpListenOptionsWithDistributionV1(original,1);assert(changed&&xl6TcpListenOptionsDistributionV1(changed)==1);xl6TcpListenOptionsDestroyV1(changed);
    changed=xl6TcpListenOptionsWithBindingV1(original,false,false,true,true);assert(changed&&!xl6TcpListenOptionsReuseAddressV1(changed)&&!xl6TcpListenOptionsReusePortV1(changed)&&xl6TcpListenOptionsExclusiveAddressV1(changed)&&xl6TcpListenOptionsIPv6OnlyV1(changed));xl6TcpListenOptionsDestroyV1(changed);
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpListenOptionsWithAcceptQueueV1(original,1025,2));errorEnd(&error,XERR_ARGUMENT);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpListenOptionsWithBindingV1(original,true,false,true,false));errorEnd(&error,XERR_ARGUMENT);
    assert(xl6ErrorScopeBegin(&error));assert(!xl6TcpListenOptionsWithBacklogV1(original,(int64_t)INT_MAX+1));errorEnd(&error,XERR_RANGE);
    handle(&original,xl6TcpListenOptionsHandlePolicyV1Get());
}
#endif
#if TEST_ROOT==3||TEST_ROOT==4
static void projection(void)
{
    xnetaddr address;assert(xrtNetAddrLoopback(&address,6,4321));void* input=xl6TcpListenOptionsCreateAddressV1(&address);assert(input);
    void* copy=xl6TcpListenOptionsAddressV1(input);assert(copy);xl6TcpListenOptionsDestroyV1(input);
    assert(xl6NetAddressFamilyV1(copy)==6&&xl6NetAddressPortV1(copy)==4321);handle(&copy,xl6NetAddressHandlePolicyV1Get());
}
#endif
#if TEST_ROOT==4
typedef struct OwnedSlot {void* raw;void (*drop)(void*);} OwnedSlot;
static void prefixes(void)
{
    size_t terminal=SIZE_MAX;
    for(size_t point=0;point<256;++point){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        OwnedSlot slots[32];size_t count=0;xvalue* boxed=NULL;
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(point));
#define KEEP(Kind,Expression) do{void* p=(Expression);if(p)slots[count++]=(OwnedSlot){p,xl6Tcp##Kind##OptionsDestroyV1};if(!p||scope.failed||xrtGetError()){if(!scope.failed)xl6TcpOptionsFailedV1();goto done;}}while(0)
        KEEP(Stream,xl6TcpStreamOptionsCreateV1());
        KEEP(Stream,xl6TcpStreamOptionsWithReadBufferV1(slots[0].raw,3,13));
        KEEP(Stream,xl6TcpStreamOptionsWithWriteWatermarksV1(slots[1].raw,11,2,31));
        KEEP(Stream,xl6TcpStreamOptionsWithConnectTimeoutV1(slots[2].raw,17));
        KEEP(Stream,xl6TcpStreamOptionsWithReadModeV1(slots[3].raw,2));
        KEEP(Stream,xl6TcpStreamOptionsWithNoDelayV1(slots[4].raw,false));
        KEEP(Stream,xl6TcpStreamOptionsWithKeepAliveV1(slots[5].raw,true));
        KEEP(Dial,xl6TcpDialOptionsCreateV1());
        KEEP(Dial,xl6TcpDialOptionsWithStreamV1(slots[7].raw,slots[6].raw));
        KEEP(Dial,xl6TcpDialOptionsWithFamilyV1(slots[8].raw,6));
        KEEP(Dial,xl6TcpDialOptionsWithAffinityV1(slots[9].raw,37));
        KEEP(Dial,xl6TcpDialOptionsWithTimeoutV1(slots[10].raw,73));
        KEEP(Dial,xl6TcpDialOptionsWithFallbackDelayV1(slots[11].raw,0));
        KEEP(Dial,xl6TcpDialOptionsWithMaxAttemptsV1(slots[12].raw,4));
        KEEP(Stream,xl6TcpDialOptionsStreamV1(slots[13].raw));
        KEEP(Listen,xl6TcpListenOptionsCreateV1());
        KEEP(Listen,xl6TcpListenOptionsWithStreamV1(slots[15].raw,slots[14].raw));
        KEEP(Listen,xl6TcpListenOptionsWithBindingV1(slots[16].raw,false,false,true,false));
        boxed=xl6NativeHandleTakeV1(&slots[17].raw,xl6TcpListenOptionsHandlePolicyV1Get());
        if(!boxed||scope.failed||xrtGetError()){if(!scope.failed)xl6TcpOptionsFailedV1();goto done;}
done:;
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtValueRelease(boxed);
        for(size_t i=count;i--!=0;)slots[i].drop(slots[i].raw);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);assert(hit?!outcome.ok&&xrtErrorKind(outcome.error)==XERR_MEMORY:outcome.ok&&!outcome.error);
        xrtErrorFree(outcome.error);assert(!xrtGetError());balanced(before);if(!hit){terminal=point;break;}
#undef KEEP
    }
    assert(terminal!=SIZE_MAX);printf("TCP options owned prefixes=%zu first-no-hit=%zu balanced\n",terminal+1,terminal);
}
#endif
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);stream();
#if TEST_ROOT==1||TEST_ROOT==4
    dial();
#endif
#if TEST_ROOT>=2
    listen();
#endif
#if TEST_ROOT==3||TEST_ROOT==4
    projection();
#endif
#if TEST_ROOT==4
    prefixes();
#endif
    assert(!xrtGetError());balanced(before);puts("TCP options offline lifecycle balanced");return 0;
}
