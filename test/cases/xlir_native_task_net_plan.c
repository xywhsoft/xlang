#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_TASK_NET
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Actual owners only: Root holds the Engine creator and Future observer;
 * task user data holds Root, and successful payloads independently hold Root.
 * Keep the constructor until the first ROOTED plan has acquired real pins,
 * then release it and re-evaluate reachability. No synthetic RC subtraction. */
typedef struct Root {
    volatile int32 refs;
    xnetengine* engine;
    xfuture* future;
    const void* claim;
    unsigned mode, active, runs, drops;
    bool cleared, finalized, resurrect;
} Root;
static Root* escaped;
static unsigned cycles, runs, data_drops, payload_drops, finalizers, freed, refusals, resurrections, pending_graphs, retries;
static void begin(xrtownershipscope* s){assert(xrtOwnershipMutationBegin(s));}
static void end(xrtownershipscope* s){assert(xrtOwnershipScopeEnd(s));}
static void freeze_begin(xrtownershipscope* s)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(s)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void idle_freeze(xnetengine* engine,xrtownershipscope* s)
{
    xrtownershipref ref=xrtNetEngineOwnership(engine);size_t count;xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){freeze_begin(s);if(ref.Ops->Count(ref.Data,&count))return;
        end(s);assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static bool root_hold(const void* data)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);
    bool ok=!root->cleared&&xrtRefRetain(&root->refs)>0;end(&s);return ok;
}
static void root_drop(const void* data)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);int32 refs=xrtRefRelease(&root->refs);assert(refs>=0);
    if(!refs){assert(!root->engine&&!root->future&&!root->active&&root->finalized);xrtFree(root);++freed;}
    end(&s);
}
static bool root_count(const void* data,size_t* count)
{
    const Root* root=data;if(root->cleared||root->active||root->refs<=0)return false;
    *count=(size_t)root->refs;return true;
}
static bool root_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const Root* root=data;
    return visit(xrtNetEngineOwnership(root->engine),context)&&visit(xrtFutureOwnership(root->future),context);
}
static const xrtownershipops root_ops={root_count,root_trace};
static xrtownershipref root_ref(const Root* root){return (xrtownershipref){root,&root_ops};}
static bool root_claim(const void* data,const void* token)
{
    Root* root=(Root*)data;if(root->claim&&root->claim!=token)return false;
    root->claim=token;return true;
}
static void root_restore(const void* data,const void* token)
{Root* root=(Root*)data;assert(root->claim==token&&!root->cleared);root->claim=NULL;}
static bool root_finalize(const void* data,const void* token)
{
    Root* root=(Root*)data;assert(root->claim==token&&!root->active&&!root->cleared&&!root->finalized&&root->drops==1);
    xfuturestate expected=root->mode==2?XFUTURE_CANCELLED:
        (root->mode==1||root->mode==3)?XFUTURE_FAILED:XFUTURE_RESOLVED;
    assert(xrtFutureState(root->future)==expected);
    assert(root->runs==((root->mode==1||root->mode==2)?0u:1u));
    assert(xrtNetEngineState(root->engine)==XNET_ENGINE_DESTROYING);
    root->finalized=true;++finalizers;return true;
}
static void root_clear(const void* data,const void* token)
{Root* root=(Root*)data;assert(root->claim==token&&root->finalized&&!root->cleared);root->cleared=true;}
static bool root_finish(const void* data,const void* token)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);assert(root->claim==token&&root->cleared);
    xnetengine* engine=root->engine;xfuture* future=root->future;root->engine=NULL;root->future=NULL;end(&s);
    assert(xrtNetEngineTryDestroy(engine)==XNET_RETIRE_READY);xrtFutureDestroy(future);return true;
}
static const xrtownershipadapterv1 root_adapter={sizeof(root_adapter),root_hold,root_drop,
    root_claim,root_restore,root_finalize,root_clear,root_finish};
static void payload_drop(ptr data,ptr unused)
{assert(!unused);++payload_drops;root_drop(data);}
static bool payload_trace(const void* data,const void* unused,xrtownershipvisitor visit,ptr context)
{return !unused&&visit(root_ref(data),context);}
static const xfuturepayloadownershipv1 payload_policy={sizeof(payload_policy),payload_drop,payload_trace};
static xtaskoutcome run(xnetworker* worker,xcancel* cancel,ptr data,xtaskvalue* result)
{
    Root* root=data;xrtownershipscope s={0};begin(&s);++root->active;
    assert(xrtNetWorkerEngine(worker)==root->engine&&cancel&&!root->cleared&&!root->finalized);
    ++root->runs;++runs;result->Value=root;
    if(root->mode!=3){assert(root_hold(root));result->Destroy=payload_drop;}
    /* mode 3 deliberately returns a borrowed non-null result; core task
     * validation must fail it before destroying Data or certifying payload. */
    --root->active;end(&s);return XTASK_SUCCESS;
}
static void data_drop(ptr data,ptr unused)
{
    Root* root=data;xrtownershipscope s={0};begin(&s);++root->active;
    assert(!unused&&!root->cleared&&!root->finalized&&root->drops==0);
    ++root->drops;++data_drops;
    if(root->resurrect){assert(!escaped&&root_hold(root));escaped=root;}
    --root->active;end(&s);root_drop(data);
}
static const xtasknetdataownershipv1 data_policy={sizeof(data_policy),run,data_drop,&root_ops};
static const xtasknetdataownershipv1 wrong_policy={sizeof(wrong_policy),run,data_drop,&root_ops};
static bool resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** output,
    const xrtownershippreparationv1** preparation)
{
    const xnettaskownershipv1* post=xrtTaskNetPostPolicyV1Get();
    const xnettimerownershipv1* timer=xrtTaskNetTimerPolicyV1Get();
    const xtaskdataownershipv1* job=xrtTaskNetJobDataPolicyV1Get();
    const xtasknetdataownershipv1* user=*(bool*)data?&wrong_policy:&data_policy;
    const xcancelwatchownershipv1* watch=xrtTaskNetCancelPolicyV1Get();
    const xfutureproducerownershipv1* producer=xrtTaskProducerPolicyV1Get();
    const xfuturepayloadownershipv1* payload=&payload_policy;
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.ProducerPolicies=&producer;admission.ProducerPolicyCount=1;
    admission.PayloadPolicies=&payload;admission.PayloadPolicyCount=1;
    admission.CancelWatchPolicies=&watch;admission.CancelWatchPolicyCount=1;
    const xrtownershipadapterv1* adapter=NULL;*preparation=NULL;
    if(ref.Ops==&root_ops)adapter=&root_adapter;
    else if((adapter=xrtNetBufPoolOwnershipAdapterV1(ref))){}
    else if((adapter=xrtNetEngineOwnershipAdapterV1(ref,&post,1,&timer,1,preparation))){}
    else if((adapter=xrtTaskNetOwnershipAdapterV1(ref,&user,1,preparation))){}
    else if((adapter=xrtTaskOwnershipAdapterV1(ref,&job,1,preparation))){}
    else if((adapter=xrtFutureOwnershipAdapterV4(ref,&admission,preparation))){}
    else if((adapter=xrtCancelOwnershipAdapterV2(ref,&watch,1,preparation))){}
    else if((adapter=xrtCancelWatchOwnershipAdapterV1(ref,&watch,1,preparation))){}
    else adapter=xrtErrorOwnershipAdapterV1(ref);
    *output=adapter;return adapter!=NULL;
}
static XL6_OwnershipPlanResult create_plan(Root* root,bool* rejected,XL6_OwnershipPlan** plan)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCreateV2(root_ref(root),resolve,rejected,plan);
        if(state>XL6_OWNERSHIP_PLAN_BUSY||(*rejected&&state==XL6_OWNERSHIP_PLAN_ERROR))return state;
        assert(!xrtDeadlineExpired(deadline));xrtClearError();++retries;xrtThreadYield();}
}
static void destroy_plan(XL6_OwnershipPlan* plan)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(xl6OwnershipPlanTryDestroy(plan)!=XL6_OWNERSHIP_PLAN_READY){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static XL6_OwnershipPlanResult settle(Root* root,XL6_OwnershipPlan* plan)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        XL6_OwnershipPlanResult state=xl6OwnershipPlanTryRefresh(plan);
        if(state==XL6_OWNERSHIP_PLAN_ROOTED||state==XL6_OWNERSHIP_PLAN_FINALIZERS||state==XL6_OWNERSHIP_PLAN_READY)return state;
        if(xrtDeadlineExpired(deadline)){fprintf(stderr,"TaskNet settle timeout: mode=%u step=%d state=%d runs=%u drops=%u\n",
            root->mode,(int)step,(int)state,root->runs,root->drops);abort();}
        xrtClearError();++retries;xrtThreadYield();}
}
static void one(unsigned mode,bool resurrect)
{
    xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
    Root* root=xrtCalloc(1,sizeof(*root));assert(root);root->refs=1;root->mode=mode;root->resurrect=resurrect;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.NodeCacheBytes=0;
    root->engine=xrtNetEngineCreate(&config);assert(root->engine&&xrtNetEngineStart(root->engine));
    bool delayed=mode==1||mode==2||mode==4;
    xrtownershipscope freeze={0};idle_freeze(root->engine,&freeze);assert(root_hold(root));
    root->future=delayed?xrtTaskNetAfterDataPolicyV1(root->engine,0,root,NULL,&data_policy,&payload_policy,
        mode==4?0:3600000000ULL):xrtTaskNetDataPolicyV1(root->engine,0,root,NULL,&data_policy,&payload_policy);
    assert(root->future);xrtownershipref ref=root_ref(root);xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==(delayed?8u:7u)&&graph.ExternalRootCount==1);
    ++pending_graphs;end(&freeze);
    /* A zero-deadline timer must actually execute successfully. Do not let
     * planner shutdown win first and count a CLOSED timer as this positive. */
    if(mode==4)assert(xrtFutureWaitFor(root->future,5000000)==XWAIT_OK);
    bool rejected=mode==1||mode==2;XL6_OwnershipPlan* plan=NULL;
    if(rejected){assert(create_plan(root,&rejected,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan);++refusals;xrtClearError();rejected=false;}
    assert(create_plan(root,&rejected,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    if(mode==2)assert(xrtFutureCancel(root->future));
    root_drop(root);XL6_OwnershipPlanResult state=settle(root,plan);
    if(resurrect){assert(state==XL6_OWNERSHIP_PLAN_ROOTED&&escaped==root&&!root->finalized);
        destroy_plan(plan);plan=NULL;++resurrections;
        assert(create_plan(root,&rejected,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
        root_drop(escaped);escaped=NULL;state=settle(root,plan);}
    assert(state==XL6_OWNERSHIP_PLAN_FINALIZERS&&!root->finalized);
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){XL6_OwnershipStepResult step=xl6OwnershipPlanFinalizeV2(plan);assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        state=xl6OwnershipPlanTryCommit(plan);if(state==XL6_OWNERSHIP_PLAN_READY)break;
        assert(state==XL6_OWNERSHIP_PLAN_BUSY||state==XL6_OWNERSHIP_PLAN_FINALIZERS);
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    assert(root->finalized&&root->drops==1);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY&&xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    destroy_plan(plan);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);++cycles;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned i=0;i<8;++i)for(unsigned mode=0;mode<5;++mode){one(mode,false);one(mode,true);}
    assert(cycles==80&&runs==48&&data_drops==80&&payload_drops==32&&finalizers==80&&freed==80&&
        refusals==32&&resurrections==40&&pending_graphs==80);
    assert(xrtMemDebugReset());
    printf("TaskNet real XLIR plans: cycles=%u runs=%u data-drops=%u payload-drops=%u finalizers=%u freed=%u refusals=%u resurrections=%u pending-graphs=%u retries=%u; exact memory; not full service/module/unload proof\n",
        cycles,runs,data_drops,payload_drops,finalizers,freed,refusals,resurrections,pending_graphs,retries);return 0;
}
