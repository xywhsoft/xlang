/* Immutable configuration/leaf ownership/OOM only. Creates no Engine,
 * Socket, thread or external I/O. Native Server defaults are reused. */
#include "src/runtime/xl6_tcp_server_options_internal.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#c);abort();}}while(0)
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount&&before.OverflowCount==after.OverflowCount&&before.UnderflowCount==after.UnderflowCount);
}
static void rejected(xl6_error_scope* scope,xerrkind kind)
{xl6_error_result result=xl6ErrorScopeEnd(scope);CHECK(!result.ok&&xrtErrorKind(result.error)==kind);xrtErrorFree(result.error);CHECK(!xrtGetError());}
int main(void)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope warm={0};CHECK(xl6ErrorScopeBegin(&warm));
    xl6_error_result error=xl6ErrorScopeEnd(&warm);CHECK(error.ok);xrtErrorFree(error.error);
    xmemdebugsnapshot initial;xrtMemDebugSnapshot(&initial);
    xnetlistenconfig a,b,c;xrtNetListenConfigInit(&a);a.Backlog=11;b=a;b.Backlog=22;c=a;c.Backlog=33;
    void* first=xl6TcpServerOptionsCreateListenV1(&a);CHECK(first);
    void* two=xl6TcpServerOptionsWithAddedEndpointV1(first,&b);CHECK(two);
    void* three=xl6TcpServerOptionsWithAddedEndpointV1(two,&c);CHECK(three);
    const xnetserverconfig *t=xl6TcpServerOptionsNativeV1(two),*v=xl6TcpServerOptionsNativeV1(three);
    CHECK(v->Additional&&v->Additional!=t->Additional&&v->Additional!=&b&&v->AdditionalCount==2);
    CHECK(v->Listen.Backlog==11&&v->Additional[0].Backlog==22&&v->Additional[1].Backlog==33);
    CHECK(xl6TcpServerOptionsEndpointCountV1(first)==1&&xl6TcpServerOptionsEndpointCountV1(three)==3);
    xnetserverconfig defaults;xrtNetServerConfigInit(&defaults);void* plain=xl6TcpServerOptionsCreateV1();CHECK(plain);
    CHECK(xl6TcpServerOptionsModeV1(plain)==defaults.Mode&&xl6TcpServerOptionsSharedPortV1(plain)==defaults.SharedPort&&
        xl6TcpServerOptionsAcceptQueueLimitV1(plain)==defaults.AcceptQueueLimit);xl6TcpServerOptionsDestroyV1(plain);
    for(int64_t index=0;index<3;++index){void* cut=xl6TcpServerOptionsWithoutEndpointV1(three,index);CHECK(cut);
        const xnetserverconfig* next=xl6TcpServerOptionsNativeV1(cut);CHECK(next->AdditionalCount==1);
        CHECK(next->Listen.Backlog==(index==0?22:11));CHECK(next->Additional[0].Backlog==(index==2?22:33));xl6TcpServerOptionsDestroyV1(cut);}
    void* copy=xl6TcpServerOptionsEndpointV1(three,1);CHECK(copy&&((xnetlistenconfig*)copy)->Backlog==22);
    void* changed=xl6TcpServerOptionsWithEndpointV1(three,1,&c);CHECK(changed);
    CHECK(xl6TcpServerOptionsNativeV1(changed)->Additional[0].Backlog==33&&v->Additional[0].Backlog==22);xl6TcpServerOptionsDestroyV1(changed);
    changed=xl6TcpServerOptionsWithModeV1(three,XNET_SERVER_REUSE_PORT);CHECK(changed);
    CHECK(xl6TcpServerOptionsModeV1(changed)==XNET_SERVER_REUSE_PORT);xl6TcpServerOptionsDestroyV1(changed);
    changed=xl6TcpServerOptionsWithSharedPortV1(three,true);CHECK(changed&&xl6TcpServerOptionsSharedPortV1(changed));xl6TcpServerOptionsDestroyV1(changed);
    changed=xl6TcpServerOptionsWithAcceptQueueLimitV1(three,UINT32_MAX);CHECK(changed);
    CHECK(xl6TcpServerOptionsAcceptQueueLimitV1(changed)==UINT32_MAX);xl6TcpServerOptionsDestroyV1(changed);
    CHECK(xrtMemDebugFailAfter(0));CHECK(xl6TcpServerOptionsEndpointCountV1(three)==3&&xl6TcpServerOptionsModeV1(three)==XNET_SERVER_SHARED);
    CHECK(!xl6TcpServerOptionsSharedPortV1(three)&&xl6TcpServerOptionsAcceptQueueLimitV1(three)>0&&!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    for(unsigned which=0;which<6;++which){xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));void* result=NULL;
        switch(which){case 0:result=xl6TcpServerOptionsWithModeV1(three,2);break;
        case 1:result=xl6TcpServerOptionsWithAcceptQueueLimitV1(three,0);break;
        case 2:result=xl6TcpServerOptionsWithAcceptQueueLimitV1(three,INT64_MAX);break;
        case 3:result=xl6TcpServerOptionsEndpointV1(three,-1);break;
        case 4:result=xl6TcpServerOptionsWithoutEndpointV1(first,0);break;
        default:result=xl6TcpServerOptionsWithEndpointV1(three,INT64_MAX,&a);break;}
        CHECK(!result);rejected(&scope,XERR_RANGE);}
    unsigned failures=0,successes=0;
    for(unsigned which=0;which<8;++which)for(unsigned budget=0;budget<2;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));
        CHECK(xrtMemDebugFailAfter(budget));void* result=NULL;
        switch(which){case 0:result=xl6TcpServerOptionsCreateV1();break;
        case 1:result=xl6TcpServerOptionsWithAddedEndpointV1(three,&a);break;
        case 2:result=xl6TcpServerOptionsWithEndpointV1(three,0,&b);break;
        case 3:result=xl6TcpServerOptionsWithoutEndpointV1(three,1);break;
        case 4:result=xl6TcpServerOptionsEndpointV1(three,0);break;
        case 5:result=xl6TcpServerOptionsWithModeV1(three,XNET_SERVER_REUSE_PORT);break;
        case 6:result=xl6TcpServerOptionsWithSharedPortV1(three,true);break;
        default:result=xl6TcpServerOptionsWithAcceptQueueLimitV1(three,4);break;}
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();error=xl6ErrorScopeEnd(&scope);
        CHECK((result!=NULL)==!hit&&error.ok==!hit);if(hit)++failures;else ++successes;xrtErrorFree(error.error);
        if(which==4)xl6TcpListenOptionsDestroyV1(result);else xl6TcpServerOptionsDestroyV1(result);balanced(before);
    }
    CHECK(failures==8&&successes==8);
    void* owned=three;xvalue* handle=xl6NativeHandleTakeV1(&owned,xl6TcpServerOptionsHandlePolicyV1Get());CHECK(handle&&!owned);
    xrtownershipref ref=xrtValueOwnership(handle);xrtownershipscope freeze={0};size_t count=0;
    CHECK(xrtOwnershipFreezeTryBegin(&freeze)&&ref.Ops->Count(ref.Data,&count)&&count==1);
    const XL6_NativeHandlePolicyV1* policy=xl6TcpServerOptionsHandlePolicyV1Get();
    CHECK(xrtValueHandleOwnershipAdapterV1(ref,policy->Ops,policy->Trace)&&xrtOwnershipScopeEnd(&freeze));
    xrtValueRelease(handle);xl6TcpServerOptionsDestroyV1(two);xl6TcpServerOptionsDestroyV1(first);
    CHECK(((xnetlistenconfig*)copy)->Backlog==22);xl6TcpListenOptionsDestroyV1(copy);balanced(initial);CHECK(xrtMemDebugReset());
    puts("Server options: independent rebased storage, 6 range refusals, OOM budgets=16 failures=8, exact leaf, balanced; no Engine/I/O");return 0;
}
