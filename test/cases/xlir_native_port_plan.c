#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_NET_ENGINE
#if defined(__linux__)
#define XRT_MODULE_NET_PORT_URING
#endif
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xrt/xrt.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>

/* A counted host owns unique Port storage and real delivered event slots.
 * Leases own exactly one context reference; no synthetic Port RC or roots. */
typedef struct Root {
    volatile int32 refs;
    xnetport* port;
    xnetsocket listener;
    xnetsocket watches[3];
    xnetportevent events[2];
    const void* claim;
    unsigned delivered, active, attempts, released, requests, cancelled;
    bool wrong, drain, resurrect, finalized, cleared, prepared, os, watch;
} Root;
static Root* escaped;
static unsigned cycles, freed, finalizers, releases, refusals, resurrections, external_roots, busy_prepares, probes;
static unsigned os_cycles, os_completions;
static unsigned watch_cycles, watch_completions;
static const xnetportownershipv1 policy, wrong_policy;
static void begin(xrtownershipscope* s){assert(xrtOwnershipMutationBegin(s));}
static void end(xrtownershipscope* s){assert(xrtOwnershipScopeEnd(s));}
static void freeze(xrtownershipscope* s)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtOwnershipFreezeTryBegin(s)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static bool hold(const void* data)
{
    Root* r=(Root*)data;xrtownershipscope s={0};begin(&s);
    bool held=!r->cleared&&xrtRefRetain(&r->refs)>0;end(&s);return held;
}
static void drop(const void* data)
{
    Root* r=(Root*)data;xrtownershipscope s={0};begin(&s);int32 refs=xrtRefRelease(&r->refs);assert(refs>=0);
    if(!refs){assert(!r->port&&!r->listener&&!r->watches[0]&&!r->watches[1]&&!r->watches[2]&&!r->active&&!r->events[0].Ownership&&!r->events[1].Ownership);
        xrtFree(r);++freed;}
    end(&s);
}
static bool count(const void* data,size_t* refs)
{
    const Root* r=data;if(r->cleared||r->active||r->refs<=0)return false;
    *refs=(size_t)r->refs;return true;
}
static bool trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const Root* r=data;const xnetportownershipv1* admitted=r->wrong?&wrong_policy:&policy;
    if(r->port&&!xrtNetPortTraceOwnershipV1(r->port,&admitted,1,visit,context))return false;
    for(unsigned i=0;i<2;++i)if(r->events[i].Ownership&&!visit(xrtNetPortEventOwnershipV1(&r->events[i]),context))return false;
    if(r->listener&&!visit(xrtNetSocketOwnership(r->listener),context))return false;
    for(unsigned i=0;i<3;++i)if(r->watches[i]&&!visit(xrtNetSocketOwnership(r->watches[i]),context))return false;
    for(unsigned i=0;i<2;++i)if(r->events[i].Accepted&&!visit(xrtNetSocketOwnership(r->events[i].Accepted),context))return false;
    return true;
}
static const xrtownershipops ops={count,trace};
static xrtownershipref reference(Root* r){return (xrtownershipref){r,&ops};}
static bool claim(const void* data,const void* token)
{Root* r=(Root*)data;if(r->claim&&r->claim!=token)return false;r->claim=token;return true;}
static void restore(const void* data,const void* token)
{Root* r=(Root*)data;assert(r->claim==token&&!r->cleared);r->claim=NULL;}
static int32 probe(ptr data)
{
    Root* r=data;xrtownershipscope s={0};freeze(&s);size_t refs=123;
    assert(count(r,&refs)==!r->cleared);if(r->cleared)assert(refs==123);end(&s);return 0;
}
static void release(ptr data)
{
    Root* r=data;xthread* thread=xrtThreadCreate(probe,r,0);assert(thread);
    assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);++probes;
    ++r->released;++releases;drop(r);
}
static const xnetportownershipv1 policy={sizeof(policy),release,&ops};
static const xnetportownershipv1 wrong_policy={sizeof(wrong_policy),release,&ops};
static void receive(Root* r,xnetportevent* event)
{
    size_t got=19;assert(!event->Ownership);
    if(r->os)while(r->cancelled<r->requests){
        /* Cancel admission is not quiescence: the real terminal packet below
         * must still transfer the actual request's owning reference. */
        assert(xrtNetPortCancel(r->port,r->cancelled+1));++r->cancelled;}
    assert(xrtNetPortWait(r->port,event,1,xl6DeadlineAfterMillisecondsV1(5000),&got)==XNET_RESULT_OK&&got==1);
    assert(event->User==r&&event->Ownership);
    if(r->os){assert(event->Type==XNET_PORT_EVENT_ACCEPT&&event->Result==XNET_RESULT_CANCELLED&&!event->Accepted);++os_completions;}
    else if(r->watch){assert(event->Type==XNET_PORT_EVENT_READY&&(event->Flags&XNET_PORT_EVENT_WRITE)&&
        !(event->Flags&XNET_PORT_EVENT_READ));++watch_completions;}
    else assert(event->Type==XNET_PORT_EVENT_USER);
}
static bool ready(const void* data){return ((const Root*)data)->prepared;}
static xrtownershipprepareresult prepare(const void* data,const void* token)
{
    Root* r=(Root*)data;assert(r->claim==token&&!r->cleared);
    if(r->prepared)return XRT_OWNERSHIP_PREPARE_READY;
    /* A genuine not-ready turn must preserve the pending duties and leases. */
    if(!r->attempts++){++busy_prepares;return XRT_OWNERSHIP_PREPARE_BUSY;}
    xrtownershipscope s={0};begin(&s);++r->active;end(&s);
    if(r->drain)while(r->delivered<2){receive(r,&r->events[r->delivered]);++r->delivered;}
    /* Backend retirement precedes the port-owned lease drops. Collector pins
     * keep queued context references real even when this removes owner slots. */
    assert(xrtNetPortDestroy(r->port));
    if(r->listener)assert(xrtNetSocketClose(r->listener));
    for(unsigned i=0;i<3;++i)if(r->watches[i]){assert(xrtNetSocketClose(r->watches[i]));r->watches[i]=NULL;}
    begin(&s);r->port=NULL;r->listener=NULL;r->prepared=true;--r->active;end(&s);
    return XRT_OWNERSHIP_PREPARE_READY;
}
static bool finalize(const void* data,const void* token)
{
    Root* r=(Root*)data;assert(r->claim==token&&r->prepared&&!r->cleared&&!r->active);
    if(r->finalized)return true;
    r->finalized=true;++finalizers;
    if(r->resurrect){assert(!escaped&&hold(r));escaped=r;}
    return true;
}
static void clear(const void* data,const void* token)
{Root* r=(Root*)data;assert(r->claim==token&&r->prepared&&r->finalized&&!r->cleared);r->cleared=true;}
static bool finish(const void* data,const void* token)
{
    Root* r=(Root*)data;assert(r->claim==token&&r->cleared&&!r->port);
    for(unsigned i=0;i<2;++i)xrtNetPortEventReleaseV1(&r->events[i]);
    return true;
}
static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,finalize,clear,finish};
static const xrtownershippreparationv1 preparation={sizeof(preparation),&adapter,ready,prepare};
static bool resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** output,
    const xrtownershippreparationv1** prep)
{
    Root* root=data;*prep=NULL;
    if(ref.Ops==&ops){*output=&adapter;*prep=&preparation;return true;}
    *output=xrtNetSocketOwnershipAdapterV1(ref);if(*output)return true;
    const xnetportownershipv1* admitted=root->wrong?&wrong_policy:&policy;
    *output=xrtNetPortLeaseOwnershipAdapterV1(ref,&admitted,1,prep);return *output!=NULL;
}
static XL6_OwnershipPlanResult create_plan(Root* r,XL6_OwnershipPlan** plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCreateV2(reference(r),resolve,r,plan);
        if(state!=XL6_OWNERSHIP_PLAN_BUSY)return state;
        assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static XL6_OwnershipPlanResult settle(XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){assert(xl6OwnershipPlanPrepare(plan)>=XL6_OWNERSHIP_STEP_BUSY);
        XL6_OwnershipPlanResult state=xl6OwnershipPlanTryRefresh(plan);
        if(state==XL6_OWNERSHIP_PLAN_FINALIZERS||state==XL6_OWNERSHIP_PLAN_ROOTED)return state;
        assert(state>=XL6_OWNERSHIP_PLAN_BUSY&&!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void destroy_plan(XL6_OwnershipPlan* plan)
{assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);}
static XL6_OwnershipPlanResult finish_finalizers(XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){assert(xl6OwnershipPlanFinalizeV2(plan)>=XL6_OWNERSHIP_STEP_BUSY);
        XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCommit(plan);
        if(state==XL6_OWNERSHIP_PLAN_READY||state==XL6_OWNERSHIP_PLAN_ROOTED)return state;
        assert((state==XL6_OWNERSHIP_PLAN_BUSY||state==XL6_OWNERSHIP_PLAN_FINALIZERS)&&!xl6DeadlineExpiredV1(deadline));
        xrtThreadYield();}
}
static void one(unsigned mode,xnetportbackend backend,bool os,bool watch)
{
    xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
    Root* r=xrtCalloc(1,sizeof(*r));assert(r);r->refs=1;r->drain=(mode&2)!=0;r->resurrect=(mode&4)!=0;
    bool external=(mode&8)!=0;xnetportevent outside={0};xnetportconfig config;
    xrtNetPortConfigInit(&config);config.Backend=backend;r->port=xrtNetPortCreate(&config);assert(r->port);
    r->os=os;r->watch=watch;r->requests=external?3u:2u;
    if(os){xnetaddr address;r->listener=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,XNET_SOCKET_NONBLOCK);
        assert(r->listener&&xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0)&&
            xrtNetSocketBind(r->listener,&address)&&xrtNetSocketListen(r->listener,16));}
    for(unsigned i=0;i<r->requests;++i){assert(hold(r));
        if(os)assert(xrtNetPortAcceptOwnedV1(r->port,r->listener,i+1,r,r,&policy));
        else if(watch){xnetaddr address;r->watches[i]=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_DGRAM,XNET_SOCKET_NONBLOCK);
            assert(r->watches[i]&&xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0)&&xrtNetSocketBind(r->watches[i],&address));
            assert(xrtNetPortWatchOwnedV1(r->port,r->watches[i],i+1,XNET_POLL_READ|XNET_POLL_WRITE,r,r,&policy));}
        else assert(xrtNetPortPostOwnedV1(r->port,i+1,r,r,&policy));}
    if(external){while(r->delivered<2){receive(r,&r->events[r->delivered]);++r->delivered;}receive(r,&outside);}
    else if(mode&1){receive(r,&r->events[0]);r->delivered=1;}
    drop(r);xrtownershipscope s={0};freeze(&s);xrtownershipref ref=reference(r);xrtownershipresult graph={0};
    size_t nodes=watch?r->requests+1:3,edges=watch?2*r->requests+r->delivered:4;
    if(watch){nodes+=r->requests;edges+=2*r->requests;}
    else if(os){++nodes;edges+=3;}
    /* An external completed Accept lease independently roots BOTH Root and
     * its shared listening socket. Do not subtract that real socket owner. */
    size_t roots=external?(os?2u:1u):0u;
    assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==nodes&&graph.EdgeCount==edges&&graph.ExternalRootCount==roots);end(&s);
    XL6_OwnershipPlan* plan=NULL;r->wrong=true;
    assert(create_plan(r,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan&&!r->claim&&!r->finalized);++refusals;
    xrtClearError();r->wrong=false;
    XL6_OwnershipPlanResult state=create_plan(r,&plan);assert(plan);
    if(external){assert(state==XL6_OWNERSHIP_PLAN_ROOTED&&!r->prepared&&!r->finalized);
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY&&!r->finalized);
        xrtNetPortEventReleaseV1(&outside);++external_roots;}
    assert(settle(plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    state=finish_finalizers(plan);
    if(r->resurrect){assert(state==XL6_OWNERSHIP_PLAN_ROOTED&&escaped==r&&!r->cleared);
        destroy_plan(plan);plan=NULL;assert(!r->claim);
        assert(create_plan(r,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);drop(escaped);escaped=NULL;++resurrections;
        assert(settle(plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
        state=finish_finalizers(plan);}
    assert(state==XL6_OWNERSHIP_PLAN_READY&&r->cleared);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY&&xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(r->released==(external?3u:2u));destroy_plan(plan);
    xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);++cycles;if(os)++os_cycles;if(watch)++watch_cycles;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    const xnetportbackend backends[]={XNET_PORT_SELECT,
#if defined(XRT_FEATURE_NET_PORT_IOCP)
        XNET_PORT_IOCP,
#endif
    };
    for(unsigned repeat=0;repeat<2;++repeat)for(size_t b=0;b<sizeof(backends)/sizeof(*backends);++b)
        for(unsigned mode=0;mode<16;++mode)one(mode,backends[b],false,false);
    /* Keep every original Post case, then add real pending OS operations. */
#if defined(XRT_FEATURE_NET_PORT_IOCP)
    const xnetportbackend os_backend=XNET_PORT_IOCP;
#else
    const xnetportbackend os_backend=XNET_PORT_URING;
#endif
    xnetportconfig os_config;xrtNetPortConfigInit(&os_config);os_config.Backend=os_backend;
    xnetport* os_test=xrtNetPortCreate(&os_config);
    if(!os_test){fprintf(stderr,"OS_PLAN_NOT_RUN: backend=%u kind=%u system=%d\n",(unsigned)os_backend,
        (unsigned)xrtErrorKind(xrtGetError()),xrtErrorSystemCode(xrtGetError()));xrtClearError();return 77;}
    assert(xrtNetPortDestroy(os_test));
    for(unsigned repeat=0;repeat<4;++repeat)for(unsigned mode=0;mode<16;++mode)one(mode,os_backend,true,false);
    const xnetportbackend readiness[]={XNET_PORT_SELECT,
#if defined(XRT_FEATURE_NET_PORT_EPOLL)
        XNET_PORT_EPOLL,
#endif
#if defined(XRT_FEATURE_NET_PORT_KQUEUE)
        XNET_PORT_KQUEUE,
#endif
    };
    for(size_t b=0;b<sizeof(readiness)/sizeof(*readiness);++b)for(unsigned repeat=0;repeat<2;++repeat)
        for(unsigned mode=0;mode<16;++mode)one(mode,readiness[b],false,true);
    assert(freed==cycles&&finalizers==cycles&&refusals==cycles&&busy_prepares==cycles&&
        resurrections*2==cycles&&external_roots*2==cycles&&releases==cycles*2+external_roots&&probes==releases&&os_cycles==64&&os_completions==136&&
        watch_cycles==32*sizeof(readiness)/sizeof(*readiness)&&watch_completions==68*sizeof(readiness)/sizeof(*readiness));
    assert(xrtMemDebugReset());
    printf("Port real XLIR plans: cycles=%u finalizers=%u freed=%u releases=%u policy-refusals=%u resurrections=%u external-roots=%u busy-prepares=%u callback-probes=%u OS-cycles=%u OS-completions=%u watch-cycles=%u watch-completions=%u; exact memory; not full service/module/unload proof\n",
        cycles,finalizers,freed,releases,refusals,resurrections,external_roots,busy_prepares,probes,os_cycles,os_completions,watch_cycles,watch_completions);return 0;
}
