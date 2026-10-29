#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_NET_ENGINE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>

/* Real cycle: Root -> Engine -> pending Port lease -> Root. Worker pool is
 * another actual owner, not a synthetic Port node or counted worker handle. */
typedef struct Root {
    volatile int32 refs;
    xnetengine* engine;
    xnetsocket socket;
    xnetcompletion completion;
    xatomic32 installed;
    xatomic32 gate_entered,gate_release;
    const void* claim;
    unsigned kind,active,callbacks;
    char byte;
    bool wrong,resurrect,finalized,cleared,take_completion,snapshot_unavailable;
} Root;
static Root* escaped;
static unsigned cycles,freed,finalizers,releases,refusals,resurrections,rooted,callbacks,probes,os_cycles;
static unsigned snapshot_retries;
static unsigned create_snapshot_retries;
static unsigned forced_create_refusals;
static unsigned completion_takes;
static const xnetportownershipv1 policy,wrong_policy;
static void begin(xrtownershipscope* s){assert(xrtOwnershipMutationBegin(s));}
static void end(xrtownershipscope* s){assert(xrtOwnershipScopeEnd(s));}
static void freeze(xrtownershipscope* s)
{xdeadline d=xrtDeadlineAfter(5000000);while(!xrtOwnershipFreezeTryBegin(s)){assert(!xrtDeadlineExpired(d));xrtThreadYield();}}
static bool hold(const void* data)
{Root* r=(Root*)data;xrtownershipscope s={0};begin(&s);bool ok=!r->cleared&&xrtRefRetain(&r->refs)>0;end(&s);return ok;}
static void drop(const void* data)
{Root* r=(Root*)data;xrtownershipscope s={0};begin(&s);int32 refs=xrtRefRelease(&r->refs);assert(refs>=0);
 if(!refs){assert(!r->engine&&!r->socket&&!r->active);xrtFree(r);++freed;}end(&s);}
static bool count(const void* data,size_t* refs)
{const Root* r=data;if(r->cleared||r->active||r->refs<=0)return false;*refs=(size_t)r->refs;return true;}
static bool trace(const void* data,xrtownershipvisitor visit,ptr user)
{const Root* r=data;return visit(xrtNetEngineOwnership(r->engine),user)&&visit(xrtNetSocketOwnership(r->socket),user);}
static const xrtownershipops ops={count,trace};
static xrtownershipref reference(Root* r){return (xrtownershipref){r,&ops};}
static bool claim(const void* data,const void* token)
{Root* r=(Root*)data;if(r->claim&&r->claim!=token)return false;r->claim=token;return true;}
static void restore(const void* data,const void* token)
{Root* r=(Root*)data;assert(r->claim==token&&!r->cleared);r->claim=NULL;}
static bool finalize(const void* data,const void* token)
{Root* r=(Root*)data;assert(r->claim==token&&!r->cleared&&!r->active&&xrtNetEngineState(r->engine)==XNET_ENGINE_DESTROYING);
 if(r->finalized)return true;
 assert(r->callbacks==(r->kind==1?1u:0u));r->finalized=true;++finalizers;
 if(r->resurrect){assert(!escaped&&hold(r));escaped=r;}return true;}
static void clear(const void* data,const void* token)
{Root* r=(Root*)data;assert(r->claim==token&&r->finalized&&!r->cleared);r->cleared=true;}
static bool finish(const void* data,const void* token)
{Root* r=(Root*)data;assert(r->claim==token&&r->cleared);xrtownershipscope s={0};begin(&s);
 xnetengine* engine=r->engine;xnetsocket socket=r->socket;r->engine=NULL;r->socket=NULL;end(&s);
 assert(xrtNetSocketClose(socket)&&xrtNetEngineTryDestroy(engine)==XNET_RETIRE_READY);return true;}
static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,finalize,clear,finish};
static int32 probe(ptr data)
{Root* r=data;xrtownershipscope s={0};freeze(&s);size_t refs=73;assert(count(r,&refs)==!r->cleared);end(&s);return 0;}
static void release(ptr data)
{Root* r=data;xthread* thread=xrtThreadCreate(probe,r,0);assert(thread);
 assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);++probes;++releases;drop(r);}
static const xnetportownershipv1 policy={sizeof(policy),release,&ops};
static const xnetportownershipv1 wrong_policy={sizeof(wrong_policy),release,&ops};
static void complete(xnetworker* worker,const xnetportevent* event,ptr data)
{Root* r=data;xrtownershipscope s={0};begin(&s);++r->active;
 assert(worker&&r->kind==1&&event->Ownership&&event->User==&r->completion&&
     (event->Flags&XNET_PORT_EVENT_WRITE)&&!(event->Flags&XNET_PORT_EVENT_READ));
 end(&s);
 if(r->take_completion){xnetportevent owned={0};assert(hold(r));
    assert(xrtNetCompletionTakeV1(worker,event,&owned)&&!event->Ownership);
    xrtNetPortEventReleaseV1(&owned);drop(r);++completion_takes;}
 begin(&s);++r->callbacks;++callbacks;--r->active;end(&s);}
static void install(xnetworker* worker,ptr data)
{Root* r=data;xnetport* port=xrtNetWorkerPortOwnedV1(worker);assert(port&&hold(r));
 if(r->kind==2)assert(xrtNetPortRecvFromOwnedV1(port,r->socket,&r->byte,1,311,&r->completion,r,&policy));
 else assert(xrtNetPortWatchOwnedV1(port,r->socket,312,XNET_POLL_READ|(r->kind==1?XNET_POLL_WRITE:0),&r->completion,r,&policy));
 xrtAtomic32Store(&r->installed,1,XMEMORY_RELEASE);}
static void task_drop(ptr data){drop(data);}
static const xnettaskownershipv1 task_policy={sizeof(task_policy),install,task_drop,&ops};
static void admission_gate(xnetworker* worker,ptr data)
{Root* r=data;(void)worker;xdeadline d=xrtDeadlineAfter(5000000);
 xrtAtomic32Store(&r->gate_entered,1,XMEMORY_RELEASE);
 while(!xrtAtomic32Load(&r->gate_release,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(d));xrtThreadYield();}}
static const xnettaskownershipv1 gate_policy={sizeof(gate_policy),admission_gate,task_drop,&ops};
static bool resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** output,
    const xrtownershippreparationv1** preparation)
{
    Root* r=data;*preparation=NULL;
    /* This resolver runs under the plan's own freeze. The earlier readiness
     * observation is not a promise that the Worker stays parked until now.
     * Record only canonical Engine Count refusal, before policy admission;
     * never turn an unknown adapter or a policy mismatch into a retry. */
    xrtownershipref engine=xrtNetEngineOwnership(r->engine);size_t references;
    if(ref.Data==engine.Data&&ref.Ops==engine.Ops&&!ref.Ops->Count(ref.Data,&references)){
        /* A real owned Worker callback forces this refusal once in selected
         * cases. Release it only after observing Count under this freeze. */
        if(xrtAtomic32Load(&r->gate_entered,XMEMORY_ACQUIRE)&&!xrtAtomic32Load(&r->gate_release,XMEMORY_ACQUIRE)){
            ++forced_create_refusals;xrtAtomic32Store(&r->gate_release,1,XMEMORY_RELEASE);
        }
        r->snapshot_unavailable=true;return false;
    }
    *output=ref.Ops==&ops?&adapter:xrtNetBufPoolOwnershipAdapterV1(ref);
    if(!*output)*output=xrtNetSocketOwnershipAdapterV1(ref);
    const xnettaskownershipv1* task=&task_policy;
    const xnetportownershipv1* admitted=r->wrong?&wrong_policy:&policy;
    if(!*output)*output=xrtNetEngineOwnershipAdapterV2(ref,&task,1,NULL,0,&admitted,1,preparation);
    if(!*output)*output=xrtNetPortLeaseOwnershipAdapterV1(ref,&admitted,1,preparation);
    return *output!=NULL;
}
static XL6_OwnershipPlanResult create_plan(Root* r,XL6_OwnershipPlan** plan)
{xdeadline d=xrtDeadlineAfter(5000000);for(;;){r->snapshot_unavailable=false;
 XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCreateV2(reference(r),resolve,r,plan);
 if(state==XL6_OWNERSHIP_PLAN_ERROR&&r->snapshot_unavailable){
    assert(!*plan&&xrtErrorKind(xrtGetError())==XERR_STATE);++create_snapshot_retries;xrtClearError();
 }else if(state!=XL6_OWNERSHIP_PLAN_BUSY)return state;
 assert(!xrtDeadlineExpired(d));xrtThreadYield();}}
static XL6_OwnershipPlanResult settle(XL6_OwnershipPlan* plan)
{xdeadline d=xrtDeadlineAfter(5000000);for(;;){XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);
 /* The public protocol distinguishes transient snapshot refusal while a
  * worker is leaving Wait from FAILED semantic preparation. Keep the same
  * plan/pins; retry only the read-only snapshot, with a finite deadline. */
 assert(step!=XL6_OWNERSHIP_STEP_FAILED);
 if(step==XL6_OWNERSHIP_STEP_ERROR)++snapshot_retries;
 XL6_OwnershipPlanResult state=xl6OwnershipPlanTryRefresh(plan);
 if(state==XL6_OWNERSHIP_PLAN_FINALIZERS||state==XL6_OWNERSHIP_PLAN_ROOTED)return state;
 assert(!xrtDeadlineExpired(d));xrtClearError();xrtThreadYield();}}
static XL6_OwnershipPlanResult finalizers_commit(XL6_OwnershipPlan* plan)
{xdeadline d=xrtDeadlineAfter(5000000);for(;;){assert(xl6OwnershipPlanFinalizeV2(plan)>=XL6_OWNERSHIP_STEP_BUSY);
 XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCommit(plan);
 if(state==XL6_OWNERSHIP_PLAN_READY||state==XL6_OWNERSHIP_PLAN_ROOTED)return state;
 assert((state==XL6_OWNERSHIP_PLAN_BUSY||state==XL6_OWNERSHIP_PLAN_FINALIZERS)&&!xrtDeadlineExpired(d));xrtThreadYield();}}
static void destroy_plan(XL6_OwnershipPlan* plan)
{assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);}
static void one(xnetportbackend backend,unsigned kind,unsigned mode)
{
    xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
    Root* r=xrtCalloc(1,sizeof(*r));assert(r);r->refs=1;r->kind=kind;r->resurrect=(mode&2)!=0;r->take_completion=(mode&1)!=0;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;config.IdleWait=1000;
    r->engine=xrtNetEngineCreate(&config);assert(r->engine&&xrtNetEngineStart(r->engine));
    r->socket=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_DGRAM,XNET_SOCKET_NONBLOCK);xnetaddr address;
    assert(r->socket&&xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0)&&xrtNetSocketBind(r->socket,&address));
    xrtNetCompletionInit(&r->completion,complete,r);assert(hold(r)&&xrtNetEnginePostOwnedV1(r->engine,0,r,&task_policy));
    xdeadline deadline=xrtDeadlineAfter(5000000);xrtownershipscope s={0};
    for(;;){freeze(&s);size_t refs;xrtownershipref engine=xrtNetEngineOwnership(r->engine);
        if(xrtAtomic32Load(&r->installed,XMEMORY_ACQUIRE)&&r->callbacks==(kind==1?1u:0u)&&
            engine.Ops->Count(engine.Data,&refs)&&r->refs==2)break;
        end(&s);assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    xrtownershipref root=reference(r);xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&graph.NodeCount==5&&graph.EdgeCount==6&&graph.ExternalRootCount==1);end(&s);
    XL6_OwnershipPlan* plan=NULL;
    if(mode&1){r->wrong=true;assert(create_plan(r,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan);r->wrong=false;xrtClearError();++refusals;}
    if(mode==0&&kind==0){
        assert(hold(r)&&xrtNetEnginePostOwnedV1(r->engine,0,r,&gate_policy));
        while(!xrtAtomic32Load(&r->gate_entered,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    }
    assert(create_plan(r,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);++rooted;
    /* The constructor really owns this reference until the plan pins it. */
    drop(r);assert(settle(plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    XL6_OwnershipPlanResult state=finalizers_commit(plan);
    if(r->resurrect){assert(state==XL6_OWNERSHIP_PLAN_ROOTED&&escaped==r);destroy_plan(plan);plan=NULL;++resurrections;
        assert(create_plan(r,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);drop(escaped);escaped=NULL;
        assert(settle(plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);state=finalizers_commit(plan);}
    assert(state==XL6_OWNERSHIP_PLAN_READY&&r->finalized);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);destroy_plan(plan);
    xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);++cycles;if(kind==2)++os_cycles;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned i=0;i<4;++i)for(unsigned mode=0;mode<4;++mode){
        one(XNET_PORT_SELECT,0,mode);one(XNET_PORT_SELECT,1,mode);
#if defined(__linux__)
        one(XNET_PORT_EPOLL,0,mode);one(XNET_PORT_EPOLL,1,mode);
#elif defined(_WIN32)
        one(XNET_PORT_IOCP,2,mode);
#endif
    }
    assert(completion_takes*2==callbacks&&finalizers==cycles&&freed==cycles&&releases==cycles&&probes==cycles&&rooted==cycles&&
        refusals*2==cycles&&resurrections*2==cycles&&xrtMemDebugReset());
#if defined(__linux__)
    assert(forced_create_refusals==8);
#else
    assert(forced_create_refusals==4);
#endif
    assert(create_snapshot_retries>=forced_create_refusals);
    printf("Engine Port real XLIR plans: cycles=%u finalizers=%u freed=%u releases=%u policy-refusals=%u resurrections=%u external-roots=%u callbacks=%u callback-probes=%u OS-cycles=%u completion-takes=%u snapshot-retries=%u create-snapshot-retries=%u forced-create-refusals=%u; exact memory; no transport/module/unload certification\n",
        cycles,finalizers,freed,releases,refusals,resurrections,rooted,callbacks,probes,os_cycles,completion_takes,snapshot_retries,create_snapshot_retries,forced_create_refusals);return 0;
}
