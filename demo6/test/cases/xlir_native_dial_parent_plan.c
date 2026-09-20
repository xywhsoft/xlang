/* Actual Dial/winning Stream/shared binding/candidate/Future cycles through
 * the unchanged production XLIR planner. Code stays resident: not DLL proof. */
#define main dial_parent_network_main
#include "tests/network/test_net_dial_parent_ownership.c"
#undef main
#include "src/runtime/xl6_ownership_plan.h"

typedef struct DialPlanOwner {
    volatile int32 References;
    xnetengine* Engine; xnetdial* Dial; xnetstream* Stream;
    xrt_net_dial_attempt* Attempt; xfuture* Futures[2];
    const void* Claim; bool Cleared,Finalized,Resurrect,Mixed;
    xatomic32 GateEntered,Gate,Done,Closed;
} DialPlanOwner;
static DialPlanOwner* lp_escape;
static unsigned lp_cases,lp_roots,lp_resurrections,lp_finalized,lp_freed,lp_drops,lp_busy,lp_pins;
static xrtownershipref lp_refused;
static bool lp_hold(const void* data)
{
    DialPlanOwner* p=(DialPlanOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    bool held=!p->Cleared&&xrtRefRetain(&p->References)>0;assert(xrtOwnershipScopeEnd(&s));return held;
}
static void lp_drop(const void* data)
{
    DialPlanOwner* p=(DialPlanOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    int32 refs=xrtRefRelease(&p->References);assert(refs>=0);
    if(!refs){assert(!p->Engine&&!p->Dial&&!p->Stream&&!p->Attempt&&!p->Futures[0]&&!p->Futures[1]);xrtFree(p);++lp_freed;}
    assert(xrtOwnershipScopeEnd(&s));
}
static bool lp_count(const void* data,size_t* count)
{const DialPlanOwner* p=data;int32 refs=__xrtAtomicRefLoad(&p->References);if(p->Cleared||refs<=0)return false;*count=(size_t)refs;return true;}
static bool lp_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const DialPlanOwner* p=data;size_t count;if(!visit||!lp_count(p,&count))return false;
    if(p->Engine&&!visit(xrtNetEngineOwnership(p->Engine),context))return false;
    if(p->Dial&&!visit(xrtNetDialOwnership(p->Dial),context))return false;
    if(p->Stream&&!visit(xrtNetStreamOwnership(p->Stream),context))return false;
    if(p->Attempt&&!visit((xrtownershipref){p->Attempt,&__xrtNetDialAttemptOps},context))return false;
    for(unsigned i=0;i<2;++i)if(p->Futures[i]&&!visit(xrtFutureOwnership(p->Futures[i]),context))return false;
    return true;
}
static const xrtownershipops lp_ops={lp_count,lp_trace};
static bool lp_claim(const void* data,const void* token)
{DialPlanOwner* p=(DialPlanOwner*)data;if(!token||p->Cleared||(p->Claim&&p->Claim!=token))return false;p->Claim=token;return true;}
static void lp_restore(const void* data,const void* token)
{DialPlanOwner* p=(DialPlanOwner*)data;assert(token&&p->Claim==token&&!p->Cleared);p->Claim=NULL;}
static bool lp_finalize(const void* data,const void* token)
{
    DialPlanOwner* p=(DialPlanOwner*)data;assert(token&&p->Claim==token&&!p->Cleared);if(p->Finalized)return true;
    xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    assert(xrtAtomic32Load(&p->Done,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&p->Closed,XMEMORY_ACQUIRE)==1);
    p->Finalized=true;++lp_finalized;if(p->Resurrect){assert(!lp_escape&&lp_hold(p));lp_escape=p;++lp_resurrections;}
    assert(xrtOwnershipScopeEnd(&s));return true;
}
static void lp_clear(const void* data,const void* token)
{DialPlanOwner* p=(DialPlanOwner*)data;assert(token&&p->Claim==token&&!p->Cleared&&p->Finalized);p->Cleared=true;}
static bool lp_finish(const void* data,const void* token)
{
    DialPlanOwner* p=(DialPlanOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    assert(token&&p->Claim==token&&p->Cleared);xnetengine* engine=p->Engine;xnetdial* dial=p->Dial;xnetstream* stream=p->Stream;
    xrt_net_dial_attempt* attempt=p->Attempt;xfuture* futures[2]={p->Futures[0],p->Futures[1]};
    p->Engine=NULL;p->Dial=NULL;p->Stream=NULL;p->Attempt=NULL;p->Futures[0]=NULL;p->Futures[1]=NULL;
    assert(xrtOwnershipScopeEnd(&s));for(unsigned i=0;i<2;++i)xrtFutureDestroy(futures[i]);
    if(attempt)__xrtNetDialAttemptDrop(attempt);
    xrtNetStreamDestroy(stream);xrtNetDialDestroy(dial);if(engine)assert(xrtNetEngineDestroy(engine));return true;
}
static const xrtownershipadapterv1 lp_adapter={sizeof(lp_adapter),lp_hold,lp_drop,lp_claim,lp_restore,lp_finalize,lp_clear,lp_finish};
static int32 lp_freeze_probe(ptr data)
{(void)data;xrtownershipscope s={0};dp_freeze(&s);assert(xrtOwnershipScopeEnd(&s));return 0;}
static void lp_context_drop(ptr data)
{
    xthread* t=xrtThreadCreate(lp_freeze_probe,NULL,0);assert(t&&xrtThreadWaitFor(t,5000000)==XWAIT_OK);xrtThreadDestroy(t);
    ++lp_drops;lp_drop(data);
}
static const xnetcallbackownershipv1 lp_callback={sizeof(lp_callback),lp_context_drop,&lp_ops};
static void lp_open(xnetstream* stream,ptr data)
{
    DialPlanOwner* p=data;assert(stream->CallbackOwner==p->Dial->CallbackOwner);
    xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));p->Attempt=stream->Control.Data;
    assert(p->Attempt&&__xrtNetDialAttemptHold(p->Attempt));assert(xrtOwnershipScopeEnd(&s));
}
static void lp_done(xnetdial* dial,xnetresult result,xnetstream* stream,const xerror* error,ptr data)
{
    DialPlanOwner* p=data;assert(dial==p->Dial&&result==XNET_RESULT_OK&&stream&&!error);
    xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));p->Stream=stream;assert(xrtOwnershipScopeEnd(&s));
    xrtAtomic32Store(&p->Done,1,XMEMORY_RELEASE);
}
static void lp_close(xnetstream* stream,xnetresult result,const xerror* error,ptr data)
{
    (void)result;(void)error;DialPlanOwner* p=data;assert(stream==p->Stream&&!p->Finalized&&!p->Cleared);
    xrtAtomic32FetchAdd(&p->Closed,1,XMEMORY_ACQ_REL);
}
static void lp_gate(xnetworker* worker,ptr data)
{(void)worker;DialPlanOwner* p=data;xrtAtomic32Store(&p->GateEntered,1,XMEMORY_RELEASE);dp_wait(&p->Gate,1);}
static bool lp_resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** adapter,
    const xrtownershippreparationv1** preparation)
{
    (void)data;*adapter=NULL;*preparation=NULL;const xnetcallbackownershipv1* callback=&lp_callback;
    const xnettaskownershipv1* tasks[]={&__xrtNetDialStartPolicy,&__xrtNetDialResolvePolicy,&__xrtNetDialCancelPolicy,
        &__xrtNetStreamStartPolicy,&__xrtNetStreamControlPolicy,&__xrtNetStreamWaitPolicy};
    const xnettimerownershipv1* timers[]={&__xrtNetDialDeadlinePolicy,&__xrtNetDialFallbackPolicy,&__xrtNetStreamTimerPolicy};
    const xnetportownershipv1* port=&__xrtNetStreamOperationPolicy;
    const xfutureproducerownershipv1* producer=xrtNetStreamFutureProducerPolicyV1Get();
    const xcancelwatchownershipv1* cancel=xrtNetStreamFutureCancelPolicyV1Get();
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.ProducerPolicies=&producer;admission.ProducerPolicyCount=1;admission.CancelWatchPolicies=&cancel;admission.CancelWatchPolicyCount=1;
    if(ref.Ops==&lp_ops)*adapter=&lp_adapter;
    else if((*adapter=xrtNetDialOwnershipAdapterV1(ref,preparation))!=NULL){}
    else if((*adapter=xrtNetDialAttemptOwnershipAdapterV1(ref,preparation))!=NULL){}
    else if((*adapter=xrtNetDialResolverOwnershipAdapterV1(ref,preparation))!=NULL){}
    else if((*adapter=xrtNetStreamOwnershipAdapterV1(ref,preparation))!=NULL){}
    else if((*adapter=xrtNetStreamFutureOwnershipAdapterV1(ref,preparation))!=NULL){}
    else if((*adapter=xrtNetEngineOwnershipAdapterV2(ref,tasks,6,timers,3,&port,1,preparation))!=NULL){}
    else if((*adapter=xrtNetCallbackOwnershipAdapterV1(ref,&callback,1,preparation))!=NULL){}
    else if((*adapter=xrtNetPortLeaseOwnershipAdapterV1(ref,&port,1,preparation))!=NULL){}
    else if((*adapter=xrtFutureOwnershipAdapterV4(ref,&admission,preparation))!=NULL){}
    else if((*adapter=xrtCancelOwnershipAdapterV2(ref,&cancel,1,preparation))!=NULL){}
    else if((*adapter=xrtCancelWatchOwnershipAdapterV1(ref,&cancel,1,preparation))!=NULL){}
    else if((*adapter=xrtNetSocketOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtNetBufPoolOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtNetAddrListOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtErrorOwnershipAdapterV1(ref))!=NULL){}
    if(!*adapter)lp_refused=ref;
    return *adapter!=NULL;
}
static XL6_OwnershipPlanResult lp_create(DialPlanOwner* p,XL6_OwnershipPlan** plan)
{
    xdeadline deadline=xrtDeadlineAfter(10000000);
    for(;;){XL6_OwnershipPlanResult r=xl6OwnershipPlanTryCreateV2((xrtownershipref){p,&lp_ops},lp_resolve,NULL,plan);
        if(r>XL6_OWNERSHIP_PLAN_BUSY)return r;
        if(xrtDeadlineExpired(deadline))fprintf(stderr,"Dial plan refusal: data=%p ops=%p\n",lp_refused.Data,(const void*)lp_refused.Ops);
        assert(!*plan&&!xrtDeadlineExpired(deadline));xrtClearError();xrtThreadYield();++lp_busy;}
}
static void lp_prepare(XL6_OwnershipPlan* plan)
{
    xdeadline deadline=xrtDeadlineAfter(15000000);
    for(;;){XL6_OwnershipStepResult r=xl6OwnershipPlanPrepare(plan);if(r==XL6_OWNERSHIP_STEP_READY)return;
        assert(r!=XL6_OWNERSHIP_STEP_FAILED&&!xrtDeadlineExpired(deadline));xrtClearError();xrtThreadYield();++lp_busy;}
}
static void lp_destroy(XL6_OwnershipPlan* plan)
{
    xdeadline deadline=xrtDeadlineAfter(10000000);XL6_OwnershipPlanResult r;
    do{r=xl6OwnershipPlanTryDestroy(plan);assert(r!=XL6_OWNERSHIP_PLAN_ERROR&&!xrtDeadlineExpired(deadline));
        if(r==XL6_OWNERSHIP_PLAN_BUSY)xrtThreadYield();}while(r==XL6_OWNERSHIP_PLAN_BUSY);
    assert(r==XL6_OWNERSHIP_PLAN_READY);
}
static void lp_one(xnetportbackend backend,unsigned mode)
{
    xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
    DialPlanOwner* p=xrtCalloc(1,sizeof(*p));assert(p);p->References=1;p->Mixed=(mode&1)!=0;p->Resurrect=(mode&2)!=0;
    unsigned freed=lp_freed,drops=lp_drops,finalized=lp_finalized;
    xnetsocket server=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,XNET_SOCKET_NONBLOCK);assert(server);
    xnetaddr address;assert(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0)&&xrtNetSocketBind(server,&address)&&
        xrtNetSocketLocal(server,&address)&&xrtNetSocketListen(server,4));
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Workers=1;ec.Backend=backend;ec.IdleWait=1000000;
    p->Engine=xrtNetEngineCreate(&ec);assert(p->Engine&&xrtNetEngineStart(p->Engine));
    xnetresolver* resolver=xrtNetResolverCreate(NULL);assert(resolver);
    assert(xrtNetEnginePost(p->Engine,0,lp_gate,p));dp_wait(&p->GateEntered,1);
    xnetdialconfig dc;xrtNetDialConfigInit(&dc);dc.Family=XNET_FAMILY_IPV4;xnetstreamevents events={0};events.Open=lp_open;events.Close=lp_close;
    assert(lp_hold(p));p->Dial=xrtNetDialOwnedV1(p->Engine,resolver,"127.0.0.1",address.Port,&dc,&events,p,lp_done,p,p,&lp_callback);assert(p->Dial);
    xrtAtomic32Store(&p->Gate,1,XMEMORY_RELEASE);dp_wait(&p->Done,1);assert(xrtNetResolverDestroy(resolver));
    if(p->Mixed){xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
        for(unsigned i=0;i<2;++i){p->Futures[i]=xrtNetStreamWaitAsync(p->Stream,XNET_STREAM_WAIT_CLOSE);assert(p->Futures[i]);}
        assert(xrtOwnershipScopeEnd(&s));}
    XL6_OwnershipPlan* plan=NULL;assert(lp_create(p,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);lp_destroy(plan);plan=NULL;++lp_roots;
    lp_drop(p);xrtownershipscope s={0};dp_quiet(p->Engine,&s);xrtownershipref root={p,&lp_ops};xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&graph.ExternalRootCount==0&&graph.EdgeCount>graph.NodeCount);
    assert(p->Dial->CallbackOwner==p->Stream->CallbackOwner&&__xrtAtomicRefLoad(&p->Dial->CallbackOwner->References)==2);
    const xrtownershippreparationv1* prep=NULL;const xrtownershipadapterv1* adapter=xrtNetDialOwnershipAdapterV1(xrtNetDialOwnership(p->Dial),&prep);assert(adapter);
    assert(xrtOwnershipScopeEnd(&s));assert(lp_create(p,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS&&plan);
    lp_prepare(plan);assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCommit(plan);
    if(p->Resurrect){assert(result==XL6_OWNERSHIP_PLAN_ROOTED&&lp_escape==p&&!p->Cleared&&!p->Dial->OwnershipCleared);lp_destroy(plan);plan=NULL;
        assert(lp_create(p,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);lp_drop(lp_escape);lp_escape=NULL;
        lp_prepare(plan);assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);result=xl6OwnershipPlanTryCommit(plan);}
    assert(result==XL6_OWNERSHIP_PLAN_READY&&p->Cleared&&p->Dial->OwnershipCleared&&p->Attempt->OwnershipCleared);
    const xerror* ambient=xrtGetError();assert(xrtMemDebugFailAfter(0));assert(!adapter->Hold(p->Dial));++lp_pins;
    assert(!xrtMemDebugFailTriggered()&&xrtGetError()==ambient);xrtMemDebugFailClear();
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY&&xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);lp_destroy(plan);
    assert(lp_freed==freed+1&&lp_drops==drops+1&&lp_finalized==finalized+1&&xrtNetSocketClose(server));xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);++lp_cases;
}
int main(void)
{
    assert(dial_parent_network_main()==0);
    for(unsigned mode=0;mode<4;++mode){lp_one(XNET_PORT_SELECT,mode);
        #if defined(_WIN32)||defined(_WIN64)
            lp_one(XNET_PORT_IOCP,mode);
        #else
            lp_one(XNET_PORT_EPOLL,mode);lp_one(XNET_PORT_URING,mode);
        #endif
    }
    testMemoryDebugDrain("Dial parent plan balance");
    printf("Dial parent collection: cases=%u roots=%u resurrected=%u finalized=%u freed=%u context-drops=%u retries=%u exact-memory=ok; Dial Future and DLL gates open\n",
        lp_cases,lp_roots,lp_resurrections,lp_finalized,lp_freed,lp_drops,lp_busy);
    printf("Dial parent pin refusal: no-allocation=%u ambient-error-unchanged=ok\n",lp_pins);return 0;
}
