/* Real TCP parents, including OPEN native IO, collected by the unchanged XLIR
 * planner. Test code is resident; this is not a dynamic-library unload proof. */
#define main tcp_parent_network_main
#include "tests/network/test_net_tcp_parent_ownership.c"
#undef main
#include "src/runtime/xl6_ownership_plan.h"

typedef struct TcpPlanOwner {
    volatile int32 References;
    xnetengine* Engine; /* The real creator reference, not a second shell hold. */
    xnetlistener* Listener;
    xnetstream* Stream;
    const void* Claim;
    bool Cleared, Finalized, Resurrect;
    xatomic32 ListenerCloses, StreamCloses;
} TcpPlanOwner;
static TcpPlanOwner* tp_active;
static TcpPlanOwner* tp_escape;
static unsigned tp_cases,tp_finalized,tp_freed,tp_drops,tp_resurrections,tp_roots,tp_busy;
static unsigned tp_hold_refusals;
static bool tp_hold(const void* data)
{
    TcpPlanOwner* p=(TcpPlanOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    bool held=!p->Cleared&&xrtRefRetain(&p->References)>0;assert(xrtOwnershipScopeEnd(&s));return held;
}
static void tp_drop(const void* data)
{
    TcpPlanOwner* p=(TcpPlanOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    int32 refs=xrtRefRelease(&p->References);assert(refs>=0);
    if(!refs){assert(!p->Engine&&!p->Listener&&!p->Stream);xrtFree(p);++tp_freed;}
    assert(xrtOwnershipScopeEnd(&s));
}
static bool tp_count(const void* data,size_t* count)
{
    const TcpPlanOwner* p=data;int32 refs=__xrtAtomicRefLoad(&p->References);
    if(p->Cleared||refs<=0)return false;
    *count=(size_t)refs;return true;
}
static bool tp_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const TcpPlanOwner* p=data;size_t count;
    if(!visit||!tp_count(p,&count))return false;
    return (!p->Engine||visit(xrtNetEngineOwnership(p->Engine),context))&&
        (!p->Listener||visit(xrtNetListenerOwnership(p->Listener),context))&&
        (!p->Stream||visit(xrtNetStreamOwnership(p->Stream),context));
}
static const xrtownershipops tp_ops={tp_count,tp_trace};
static bool tp_claim(const void* data,const void* token)
{
    TcpPlanOwner* p=(TcpPlanOwner*)data;if(!token||p->Cleared||(p->Claim&&p->Claim!=token))return false;
    p->Claim=token;return true;
}
static void tp_restore(const void* data,const void* token)
{TcpPlanOwner* p=(TcpPlanOwner*)data;assert(token&&p->Claim==token&&!p->Cleared);p->Claim=NULL;}
static bool tp_finalize(const void* data,const void* token)
{
    TcpPlanOwner* p=(TcpPlanOwner*)data;assert(token&&p->Claim==token&&!p->Cleared);
    if(p->Finalized)return true;
    xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));p->Finalized=true;++tp_finalized;
    /* ALL native Close duties must precede language semantic finalization. */
    assert(xrtAtomic32Load(&p->ListenerCloses,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&p->StreamCloses,XMEMORY_ACQUIRE)==(p->Stream?1u:0u));
    if(p->Resurrect){assert(!tp_escape&&tp_hold(p));tp_escape=p;++tp_resurrections;}
    assert(xrtOwnershipScopeEnd(&s));return true;
}
static void tp_clear(const void* data,const void* token)
{TcpPlanOwner* p=(TcpPlanOwner*)data;assert(token&&p->Claim==token&&p->Finalized&&!p->Cleared);p->Cleared=true;}
static bool tp_finish(const void* data,const void* token)
{
    TcpPlanOwner* p=(TcpPlanOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    assert(token&&p->Claim==token&&p->Cleared);
    xnetengine* engine=p->Engine;xnetlistener* listener=p->Listener;xnetstream* stream=p->Stream;
    p->Engine=NULL;p->Listener=NULL;p->Stream=NULL;assert(xrtOwnershipScopeEnd(&s));
    xrtNetStreamDestroy(stream);xrtNetListenerDestroy(listener);
    if(engine)assert(xrtNetEngineDestroy(engine));
    return true;
}
static const xrtownershipadapterv1 tp_adapter={sizeof(tp_adapter),tp_hold,tp_drop,tp_claim,tp_restore,tp_finalize,tp_clear,tp_finish};
static int32 tp_freeze_probe(ptr data)
{
    (void)data;xrtownershipscope scope={0};xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(&scope)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    assert(xrtOwnershipScopeEnd(&scope));return 0;
}
static void tp_context_drop(ptr data)
{
    xthread* t=xrtThreadCreate(tp_freeze_probe,NULL,0);assert(t);
    assert(xrtThreadWaitFor(t,5000000)==XWAIT_OK);xrtThreadDestroy(t);++tp_drops;tp_drop(data);
}
static const xnetcallbackownershipv1 tp_policy={sizeof(tp_policy),tp_context_drop,&tp_ops};
static void tp_listener_close(xnetlistener* listener,ptr data)
{TcpPlanOwner* p=data;assert(listener==p->Listener);xrtAtomic32FetchAdd(&p->ListenerCloses,1,XMEMORY_ACQ_REL);}
static void tp_stream_close(xnetstream* stream,xnetresult result,const xerror* error,ptr data)
{
    (void)result;(void)error;(void)data;assert(stream==tp_active->Stream);
    xrtAtomic32FetchAdd(&tp_active->StreamCloses,1,XMEMORY_ACQ_REL);
}
static bool tp_resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** adapter,
    const xrtownershippreparationv1** preparation)
{
    (void)data;*preparation=NULL;*adapter=NULL;
    const xnetcallbackownershipv1* callback=&tp_policy;
    const xnettaskownershipv1* tasks[]={&__xrtNetListenerStartPolicy,&__xrtNetListenerClosePolicy,
        &__xrtNetStreamStartPolicy,&__xrtNetStreamControlPolicy};
    const xnettimerownershipv1* timers[]={&__xrtNetListenerRetryPolicy,&__xrtNetStreamTimerPolicy};
    const xnetportownershipv1* ports[]={&__xrtNetListenerOperationPolicy,&__xrtNetStreamOperationPolicy};
    if(ref.Ops==&tp_ops)*adapter=&tp_adapter;
    else if((*adapter=xrtNetStreamOwnershipAdapterV1(ref,preparation))!=NULL){}
    else if((*adapter=xrtNetListenerOwnershipAdapterV1(ref,preparation))!=NULL){}
    else if((*adapter=xrtNetEngineOwnershipAdapterV2(ref,tasks,4,timers,2,ports,2,preparation))!=NULL){}
    else if((*adapter=xrtNetCallbackOwnershipAdapterV1(ref,&callback,1,preparation))!=NULL){}
    else if((*adapter=xrtNetPortLeaseOwnershipAdapterV1(ref,ports,2,preparation))!=NULL){}
    else if((*adapter=xrtNetSocketOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtNetBufPoolOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtErrorOwnershipAdapterV1(ref))!=NULL){}
    return *adapter!=NULL;
}
static XL6_OwnershipPlanResult tp_create(TcpPlanOwner* p,XL6_OwnershipPlan** plan)
{
    xdeadline deadline=xrtDeadlineAfter(10000000);XL6_OwnershipPlanResult result;
    for(;;){
        result=xl6OwnershipPlanTryCreateV2((xrtownershipref){p,&tp_ops},tp_resolve,NULL,plan);
        if(result>XL6_OWNERSHIP_PLAN_BUSY)return result;
        assert(!*plan&&!xrtDeadlineExpired(deadline));xrtClearError();xrtThreadYield();++tp_busy;
    }
}
static void tp_prepare(XL6_OwnershipPlan* plan)
{
    xdeadline deadline=xrtDeadlineAfter(15000000);
    for(;;){
        XL6_OwnershipStepResult result=xl6OwnershipPlanPrepare(plan);
        if(result==XL6_OWNERSHIP_STEP_READY)return;
        assert(result!=XL6_OWNERSHIP_STEP_FAILED&&!xrtDeadlineExpired(deadline));
        xrtClearError();xrtThreadYield();++tp_busy;
    }
}
static void tp_destroy(XL6_OwnershipPlan* plan)
{
    xdeadline deadline=xrtDeadlineAfter(10000000);XL6_OwnershipPlanResult result;
    do{result=xl6OwnershipPlanTryDestroy(plan);assert(result!=XL6_OWNERSHIP_PLAN_ERROR&&!xrtDeadlineExpired(deadline));
        if(result==XL6_OWNERSHIP_PLAN_BUSY)xrtThreadYield();
    }while(result==XL6_OWNERSHIP_PLAN_BUSY);
    assert(result==XL6_OWNERSHIP_PLAN_READY);
}
static void tp_one(xnetportbackend backend,unsigned mode)
{
    xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
    TcpPlanOwner* p=xrtCalloc(1,sizeof(*p));assert(p);p->References=1;p->Resurrect=(mode&2)!=0;tp_active=p;
    unsigned freed=tp_freed,drops=tp_drops,finalized=tp_finalized;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;config.IdleWait=1000000;
    p->Engine=xrtNetEngineCreate(&config);assert(p->Engine&&xrtNetEngineStart(p->Engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);listen.AcceptConcurrency=1;listen.Stream.ReadMode=XNET_STREAM_READ_DIRECT;
    assert(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    xnetlistenerevents le={0};le.Close=tp_listener_close;xnetstreamevents se={0};se.Close=tp_stream_close;
    assert(tp_hold(p));p->Listener=xrtNetListenOwnedV1(p->Engine,&listen,&le,&se,p,p,&tp_policy);assert(p->Listener);
    xnetsocket peer=NULL;
    if(mode&1){
        xnetaddr address;assert(xrtNetListenerLocal(p->Listener,&address));
        peer=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,0);assert(peer&&xrtNetSocketConnect(peer,&address)==XNET_RESULT_OK);
        wait_count(&p->Listener->QueuedAccepts,1);p->Stream=xrtNetListenerAccept(p->Listener);assert(p->Stream);
    }
    /* Caller still owns one real Context ref: ALL live service nodes root it. */
    XL6_OwnershipPlan* plan=NULL;assert(tp_create(p,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    tp_destroy(plan);plan=NULL;++tp_roots;
    tp_drop(p); /* Engine creator and parent references now belong only to the cycle. */
    xrtownershipscope scope={0};quiet(p->Engine,&scope);xrtownershipref seed={p,&tp_ops};xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&seed,1,NULL,0,&graph)&&graph.ExternalRootCount==0&&graph.EdgeCount>graph.NodeCount);
    const xrtownershippreparationv1* prepared=NULL;
    const xrtownershipadapterv1* listener_adapter=xrtNetListenerOwnershipAdapterV1(xrtNetListenerOwnership(p->Listener),&prepared);
    const xrtownershipadapterv1* stream_adapter=p->Stream?xrtNetStreamOwnershipAdapterV1(xrtNetStreamOwnership(p->Stream),&prepared):NULL;
    assert(listener_adapter&&(!p->Stream||stream_adapter));
    assert(xrtOwnershipScopeEnd(&scope));
    assert(tp_create(p,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS&&plan);
    tp_prepare(plan);assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCommit(plan);
    if(p->Resurrect){
        assert(result==XL6_OWNERSHIP_PLAN_ROOTED&&tp_escape==p&&!p->Cleared&&!p->Listener->OwnershipCleared);
        tp_destroy(plan);plan=NULL;
        assert(tp_create(p,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);tp_drop(tp_escape);tp_escape=NULL;
        tp_prepare(plan);assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
        result=xl6OwnershipPlanTryCommit(plan);
    }
    assert(result==XL6_OWNERSHIP_PLAN_READY&&p->Cleared&&p->Listener->OwnershipCleared);
    const xerror* previous_error=xrtGetError();assert(xrtMemDebugFailAfter(0));
    assert(!listener_adapter->Hold(p->Listener));++tp_hold_refusals;
    if(p->Stream){assert(!stream_adapter->Hold(p->Stream));++tp_hold_refusals;}
    assert(!xrtMemDebugFailTriggered()&&xrtGetError()==previous_error);xrtMemDebugFailClear();
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    tp_destroy(plan);
    assert(tp_freed==freed+1&&tp_drops==drops+1&&tp_finalized==finalized+1);
    if(peer)assert(xrtNetSocketClose(peer));
    xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&before.InvalidFreeCount==after.InvalidFreeCount&&
        before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);++tp_cases;
}
int main(void)
{
    assert(tcp_parent_network_main()==0);
    for(unsigned mode=0;mode<4;++mode){tp_one(XNET_PORT_SELECT,mode);
        #if defined(_WIN32)||defined(_WIN64)
            tp_one(XNET_PORT_IOCP,mode);
        #else
            tp_one(XNET_PORT_EPOLL,mode);tp_one(XNET_PORT_URING,mode);
        #endif
    }
    testMemoryDebugDrain("TCP parent plan balance");
    printf("TCP parent collection: cases=%u roots=%u resurrected=%u finalized=%u freed=%u context-drops=%u retries=%u exact-memory=ok; UDP/Dial and DLL gates open\n",
        tp_cases,tp_roots,tp_resurrections,tp_finalized,tp_freed,tp_drops,tp_busy);
    printf("TCP parent pin refusal: no-allocation=%u ambient-error-unchanged=ok\n",tp_hold_refusals);return 0;
}
