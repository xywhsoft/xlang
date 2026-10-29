/* Production XLIR planner, real OPEN UDP/Future/queued-send cycles. The test
 * deliberately leaves real attached sends queued for native Prepare to abort;
 * it uses the same allocator/linker/retirement functions, not fabricated refs.
 * Code remains resident: this is not a DLL unload or language acceptance gate. */
#define main udp_parent_network_main
#include "tests/network/test_net_udp_parent_ownership.c"
#undef main
#include "src/runtime/xl6_ownership_plan.h"

typedef struct UdpPlanOwner {
    volatile int32 References;
    xnetengine* Engine; xnetudp* Udp; xfuture* Futures[2];
    __xrt_net_udp_send* Sends[2]; xnetaddr Remote;
    const void* Claim; bool Cleared,Finalized,Resurrect,Mixed;
    xatomic32 Closes,Releases,Attached;
} UdpPlanOwner;
static UdpPlanOwner* up_escape;
static unsigned up_cases,up_finalized,up_freed,up_drops,up_resurrections,up_roots,up_busy,up_sends,up_pin_refusals;
static xrtownershipref up_last_refusal;
static bool up_hold(const void* data)
{
    UdpPlanOwner* p=(UdpPlanOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    bool held=!p->Cleared&&xrtRefRetain(&p->References)>0;assert(xrtOwnershipScopeEnd(&s));return held;
}
static void up_drop(const void* data)
{
    UdpPlanOwner* p=(UdpPlanOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    int32 refs=xrtRefRelease(&p->References);assert(refs>=0);
    if(!refs){assert(!p->Engine&&!p->Udp&&!p->Futures[0]&&!p->Futures[1]&&!p->Sends[0]&&!p->Sends[1]);xrtFree(p);++up_freed;}
    assert(xrtOwnershipScopeEnd(&s));
}
static bool up_count(const void* data,size_t* count)
{const UdpPlanOwner* p=data;int32 refs=__xrtAtomicRefLoad(&p->References);if(p->Cleared||refs<=0)return false;*count=(size_t)refs;return true;}
static bool up_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const UdpPlanOwner* p=data;size_t count;if(!visit||!up_count(p,&count))return false;
    if(p->Engine&&!visit(xrtNetEngineOwnership(p->Engine),context))return false;
    if(p->Udp&&!visit(xrtNetUdpOwnership(p->Udp),context))return false;
    for(unsigned i=0;i<2;++i){if(p->Futures[i]&&!visit(xrtFutureOwnership(p->Futures[i]),context))return false;
        if(p->Sends[i]&&!visit((xrtownershipref){p->Sends[i],&__xrtNetUdpSendNodeOps},context))return false;}
    return true;
}
static const xrtownershipops up_ops={up_count,up_trace};
static bool up_claim(const void* data,const void* token)
{UdpPlanOwner* p=(UdpPlanOwner*)data;if(!token||p->Cleared||(p->Claim&&p->Claim!=token))return false;p->Claim=token;return true;}
static void up_restore(const void* data,const void* token)
{UdpPlanOwner* p=(UdpPlanOwner*)data;assert(token&&p->Claim==token&&!p->Cleared);p->Claim=NULL;}
static bool up_finalize(const void* data,const void* token)
{
    UdpPlanOwner* p=(UdpPlanOwner*)data;assert(token&&p->Claim==token&&!p->Cleared);if(p->Finalized)return true;
    xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    assert(xrtAtomic32Load(&p->Closes,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&p->Releases,XMEMORY_ACQUIRE)==(p->Mixed?2u:0u));
    p->Finalized=true;++up_finalized;if(p->Resurrect){assert(!up_escape&&up_hold(p));up_escape=p;++up_resurrections;}
    assert(xrtOwnershipScopeEnd(&s));return true;
}
static void up_clear(const void* data,const void* token)
{UdpPlanOwner* p=(UdpPlanOwner*)data;assert(token&&p->Claim==token&&!p->Cleared&&p->Finalized);p->Cleared=true;}
static bool up_finish(const void* data,const void* token)
{
    UdpPlanOwner* p=(UdpPlanOwner*)data;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));
    assert(token&&p->Claim==token&&p->Cleared);xnetengine* engine=p->Engine;xnetudp* udp=p->Udp;
    xfuture* futures[2]={p->Futures[0],p->Futures[1]};__xrt_net_udp_send* sends[2]={p->Sends[0],p->Sends[1]};
    p->Engine=NULL;p->Udp=NULL;for(unsigned i=0;i<2;++i){p->Futures[i]=NULL;p->Sends[i]=NULL;}
    assert(xrtOwnershipScopeEnd(&s));
    for(unsigned i=0;i<2;++i){xrtFutureDestroy(futures[i]);if(sends[i])__xrtNetUdpSendNodeDrop(sends[i]);}
    xrtNetUdpDestroy(udp);if(engine)assert(xrtNetEngineDestroy(engine));return true;
}
static const xrtownershipadapterv1 up_adapter={sizeof(up_adapter),up_hold,up_drop,up_claim,up_restore,up_finalize,up_clear,up_finish};
static int32 up_freeze_probe(ptr data)
{
    (void)data;xrtownershipscope s={0};xdeadline d=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(&s)){assert(!xrtDeadlineExpired(d));xrtThreadYield();}
    assert(xrtOwnershipScopeEnd(&s));return 0;
}
static void up_probe(void)
{xthread* t=xrtThreadCreate(up_freeze_probe,NULL,0);assert(t&&xrtThreadWaitFor(t,5000000)==XWAIT_OK);xrtThreadDestroy(t);}
static void up_context_drop(ptr data){up_probe();++up_drops;up_drop(data);}
static const xnetcallbackownershipv1 up_callback={sizeof(up_callback),up_context_drop,&up_ops};
static void up_close(xnetudp* udp,xnetresult result,const xerror* error,ptr data)
{(void)result;(void)error;UdpPlanOwner* p=data;assert(udp==p->Udp);xrtAtomic32FetchAdd(&p->Closes,1,XMEMORY_ACQ_REL);}
static void up_release(ptr data,cbytes bytes,size_t size)
{
    UdpPlanOwner* p=data;assert(size==4&&!memcmp(bytes,"late",4)&&!p->Finalized&&!p->Cleared);
    up_probe();xrtAtomic32FetchAdd(&p->Releases,1,XMEMORY_ACQ_REL);++up_sends;up_drop(p);
}
static const xnetbufownershipv1 up_payload={sizeof(up_payload),up_release,&up_ops};
static void up_attach(xnetworker* worker,ptr data)
{
    UdpPlanOwner* p=data;assert(worker==p->Udp->Worker);
    for(unsigned i=0;i<2;++i){xnetresult result;__xrt_net_udp_send* send=__xrtNetUdpCreateSend(p->Udp,&p->Remote,NULL,4,false,&result);
        assert(send&&result==XNET_RESULT_OK&&up_hold(p));send->Data=(cbytes)"late";send->OwnsExternal=true;
        send->Release=up_release;send->ReleaseContext=p;send->OwnershipPolicy=&up_payload;
        assert(__xrtNetUdpAttachSend(send)&&__xrtNetUdpSendNodeHold(send));p->Sends[i]=send;
    }
    xrtAtomic32Store(&p->Attached,1,XMEMORY_RELEASE);
}
static bool up_resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** adapter,
    const xrtownershippreparationv1** preparation)
{
    (void)data;*adapter=NULL;*preparation=NULL;
    const xnetcallbackownershipv1* callback=&up_callback;const xnetbufownershipv1* payload=&up_payload;
    const xnettaskownershipv1* tasks[]={&__xrtNetUdpControlPolicy,&__xrtNetUdpWaitPolicy,&__xrtNetUdpSendJobPolicy};
    const xnetportownershipv1* ports[]={&__xrtNetUdpOperationPolicy};
    const xfutureproducerownershipv1* producer=xrtNetUdpFutureProducerPolicyV1Get();
    const xcancelwatchownershipv1* cancel=xrtNetUdpFutureCancelPolicyV1Get();
    const xfuturepayloadownershipv1* values[]={xrtNetUdpPacketPayloadPolicyV1Get(),xrtNetUdpErrorPacketPayloadPolicyV1Get(),xrtNetUdpBatchPayloadPolicyV1Get()};
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);admission.PayloadPolicies=values;admission.PayloadPolicyCount=3;
    admission.ProducerPolicies=&producer;admission.ProducerPolicyCount=1;admission.CancelWatchPolicies=&cancel;admission.CancelWatchPolicyCount=1;
    if(ref.Ops==&up_ops)*adapter=&up_adapter;
    else if((*adapter=xrtNetUdpOwnershipAdapterV1(ref,preparation))!=NULL){}
    else if((*adapter=xrtNetUdpSendOwnershipAdapterV1(ref,&payload,1,preparation))!=NULL){}
    else if((*adapter=xrtNetUdpFutureOwnershipAdapterV1(ref,preparation))!=NULL){}
    else if((*adapter=xrtNetEngineOwnershipAdapterV2(ref,tasks,3,NULL,0,ports,1,preparation))!=NULL){}
    else if((*adapter=xrtNetCallbackOwnershipAdapterV1(ref,&callback,1,preparation))!=NULL){}
    else if((*adapter=xrtNetPortLeaseOwnershipAdapterV1(ref,ports,1,preparation))!=NULL){}
    else if((*adapter=xrtFutureOwnershipAdapterV4(ref,&admission,preparation))!=NULL){}
    else if((*adapter=xrtCancelOwnershipAdapterV2(ref,&cancel,1,preparation))!=NULL){}
    else if((*adapter=xrtCancelWatchOwnershipAdapterV1(ref,&cancel,1,preparation))!=NULL){}
    else if((*adapter=xrtNetUdpBatchOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtNetUdpPacketOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtNetUdpErrorPacketOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtNetSocketOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtNetBufPoolOwnershipAdapterV1(ref))!=NULL){}
    else if((*adapter=xrtErrorOwnershipAdapterV1(ref))!=NULL){}
    if(!*adapter)up_last_refusal=ref;
    return *adapter!=NULL;
}
static XL6_OwnershipPlanResult up_create(UdpPlanOwner* p,XL6_OwnershipPlan** plan)
{
    xdeadline d=xrtDeadlineAfter(10000000);
    for(;;){XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreateV2((xrtownershipref){p,&up_ops},up_resolve,NULL,plan);
        if(result>XL6_OWNERSHIP_PLAN_BUSY)return result;
        if(xrtDeadlineExpired(d))fprintf(stderr,"UDP plan admission refused: data=%p ops=%p\n",up_last_refusal.Data,(const void*)up_last_refusal.Ops);
        assert(!*plan&&!xrtDeadlineExpired(d));xrtClearError();xrtThreadYield();++up_busy;}
}
static void up_prepare(XL6_OwnershipPlan* plan)
{
    xdeadline d=xrtDeadlineAfter(15000000);
    for(;;){XL6_OwnershipStepResult result=xl6OwnershipPlanPrepare(plan);if(result==XL6_OWNERSHIP_STEP_READY)return;
        assert(result!=XL6_OWNERSHIP_STEP_FAILED&&!xrtDeadlineExpired(d));xrtClearError();xrtThreadYield();++up_busy;}
}
static void up_destroy(XL6_OwnershipPlan* plan)
{
    xdeadline d=xrtDeadlineAfter(10000000);XL6_OwnershipPlanResult result;
    do{result=xl6OwnershipPlanTryDestroy(plan);assert(result!=XL6_OWNERSHIP_PLAN_ERROR&&!xrtDeadlineExpired(d));
        if(result==XL6_OWNERSHIP_PLAN_BUSY)xrtThreadYield();}while(result==XL6_OWNERSHIP_PLAN_BUSY);
    assert(result==XL6_OWNERSHIP_PLAN_READY);
}
static void up_one(xnetportbackend backend,unsigned mode)
{
    xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
    UdpPlanOwner* p=xrtCalloc(1,sizeof(*p));assert(p);p->References=1;p->Mixed=(mode&1)!=0;p->Resurrect=(mode&2)!=0;
    unsigned freed=up_freed,drops=up_drops,finalized=up_finalized;
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Workers=1;ec.Backend=backend;ec.IdleWait=1000000;
    p->Engine=xrtNetEngineCreate(&ec);assert(p->Engine&&xrtNetEngineStart(p->Engine));
    xnetaddr local;assert(xrtNetAddrLoopback(&local,XNET_FAMILY_IPV4,0)&&xrtNetAddrLoopback(&p->Remote,XNET_FAMILY_IPV4,9));
    xnetudpevents events={0};events.Close=up_close;assert(up_hold(p));
    p->Udp=xrtNetUdpOpenOwnedV1(p->Engine,&local,NULL,0,NULL,&events,p,p,&up_callback);assert(p->Udp);udp_wait(&p->Udp->State,XNET_UDP_OPEN);
    if(p->Mixed){p->Futures[0]=xrtNetUdpWaitAsync(p->Udp,XNET_UDP_WAIT_CLOSE);p->Futures[1]=xrtNetUdpReceiveBatchAsync(p->Udp,2);
        assert(p->Futures[0]&&p->Futures[1]&&xrtNetEnginePost(p->Engine,0,up_attach,p));udp_wait(&p->Attached,1);}
    XL6_OwnershipPlan* plan=NULL;assert(up_create(p,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);up_destroy(plan);plan=NULL;++up_roots;
    up_drop(p);xrtownershipscope scope={0};udp_quiet(p->Engine,&scope);xrtownershipref root={p,&up_ops};xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&graph.ExternalRootCount==0&&graph.EdgeCount>graph.NodeCount);
    if(p->Mixed){assert(p->Udp->SendHead==p->Sends[0]&&p->Udp->SendTail==p->Sends[1]&&p->Sends[0]->Next==p->Sends[1]);
        assert(!p->Sends[1]->Next&&!xrtAtomic32Load(&p->Releases,XMEMORY_ACQUIRE)&&!xrtAtomic64Load(&p->Udp->ActiveSends,XMEMORY_ACQUIRE));}
    const xrtownershippreparationv1* prep=NULL;const xrtownershipadapterv1* adapter=xrtNetUdpOwnershipAdapterV1(xrtNetUdpOwnership(p->Udp),&prep);assert(adapter);
    assert(xrtOwnershipScopeEnd(&scope));assert(up_create(p,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS&&plan);
    up_prepare(plan);assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCommit(plan);
    if(p->Resurrect){assert(result==XL6_OWNERSHIP_PLAN_ROOTED&&up_escape==p&&!p->Cleared&&!p->Udp->OwnershipCleared);up_destroy(plan);plan=NULL;
        assert(up_create(p,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);up_drop(up_escape);up_escape=NULL;
        up_prepare(plan);assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);result=xl6OwnershipPlanTryCommit(plan);}
    assert(result==XL6_OWNERSHIP_PLAN_READY&&p->Cleared&&p->Udp->OwnershipCleared);
    const xerror* ambient=xrtGetError();assert(xrtMemDebugFailAfter(0));assert(!adapter->Hold(p->Udp));++up_pin_refusals;
    assert(!xrtMemDebugFailTriggered()&&xrtGetError()==ambient);xrtMemDebugFailClear();
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY&&xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);up_destroy(plan);
    assert(up_freed==freed+1&&up_drops==drops+1&&up_finalized==finalized+1);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);++up_cases;
}
int main(void)
{
    assert(udp_parent_network_main()==0);
    for(unsigned mode=0;mode<4;++mode){up_one(XNET_PORT_SELECT,mode);
        #if defined(_WIN32)||defined(_WIN64)
            up_one(XNET_PORT_IOCP,mode);
        #else
            up_one(XNET_PORT_EPOLL,mode);up_one(XNET_PORT_URING,mode);
        #endif
    }
    testMemoryDebugDrain("UDP parent plan balance");
    printf("UDP parent collection: cases=%u roots=%u resurrected=%u finalized=%u freed=%u context-drops=%u retired-sends=%u retries=%u exact-memory=ok; Dial and DLL gates open\n",
        up_cases,up_roots,up_resurrections,up_finalized,up_freed,up_drops,up_sends,up_busy);
    printf("UDP parent pin refusal: no-allocation=%u ambient-error-unchanged=ok\n",up_pin_refusals);return 0;
}
