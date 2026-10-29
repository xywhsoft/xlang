#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_NET_ENGINE
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xrt/xrt.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Root owns the actual Engine creator. Accepted task and timer each own one
 * Root reference. The constructor reference is really dropped; there are no
 * synthetic internal roots, worker counts or special planner implementations. */
typedef struct Root {
    volatile int32 refs;
    xnetengine* engine;
    xnetpost post;
    const void* claim;
    unsigned active, tasks, timers;
    bool cleared, finalized, resurrect, embedded;
} Root;
static Root* escaped;
static unsigned cycles, tasks, timers, finalizers, freed, refusals, resurrections, heap_plans, retries, embedded_plans;
static void begin(xrtownershipscope* s) { assert(xrtOwnershipMutationBegin(s)); }
static void end(xrtownershipscope* s) { assert(xrtOwnershipScopeEnd(s)); }
static void freeze_begin(xrtownershipscope* s)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtOwnershipFreezeTryBegin(s)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void idle_freeze(xnetengine* engine,xrtownershipscope* s)
{
    xrtownershipref ref=xrtNetEngineOwnership(engine);size_t count;XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){freeze_begin(s);if(ref.Ops->Count(ref.Data,&count))return;
        end(s);assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static bool root_hold(const void* data)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);
    bool ok=!root->cleared&&xrtRefRetain(&root->refs)>0;end(&s);return ok;
}
static void root_drop(const void* data)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);int32 refs=xrtRefRelease(&root->refs);assert(refs>=0);
    if(!refs){assert(!root->engine&&!root->active);xrtFree(root);++freed;}
    end(&s);
}
static bool root_count(const void* data,size_t* count)
{
    const Root* root=data;if(root->cleared||root->active||root->refs<=0)return false;
    *count=(size_t)root->refs;return true;
}
static bool root_trace(const void* data,xrtownershipvisitor visit,ptr context)
{return visit(xrtNetEngineOwnership(((const Root*)data)->engine),context);}
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
    Root* root=(Root*)data;assert(root->claim==token&&!root->active&&!root->cleared&&!root->finalized);
    assert(root->tasks==1&&root->timers==1&&xrtNetEngineState(root->engine)==XNET_ENGINE_DESTROYING);
    root->finalized=true;++finalizers;return true;
}
static void root_clear(const void* data,const void* token)
{Root* root=(Root*)data;assert(root->claim==token&&root->finalized&&!root->cleared);root->cleared=true;}
static bool root_finish(const void* data,const void* token)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);assert(root->claim==token&&root->cleared);
    xnetengine* engine=root->engine;root->engine=NULL;end(&s);
    assert(xrtNetEngineTryDestroy(engine)==XNET_RETIRE_READY);return true;
}
static const xrtownershipadapterv1 root_adapter={sizeof(root_adapter),root_hold,root_drop,
    root_claim,root_restore,root_finalize,root_clear,root_finish};
static void task(xnetworker* worker,ptr data)
{
    Root* root=data;xrtownershipscope s={0};begin(&s);++root->active;
    assert(xrtNetWorkerEngine(worker)==root->engine&&!root->cleared&&!root->finalized);
    if(root->embedded)assert(!xrtNetPostPending(&root->post));
    ++root->tasks;++tasks;--root->active;end(&s);
}
static void timer(xnetworker* worker,uint64 id,xnetresult result,ptr data)
{
    Root* root=data;xrtownershipscope s={0};begin(&s);++root->active;
    assert(worker&&id&&result==XNET_RESULT_CLOSED&&!root->cleared&&!root->finalized);
    if(root->resurrect){assert(!escaped&&root_hold(root));escaped=root;}
    ++root->timers;++timers;--root->active;end(&s);
}
static void release(ptr data){root_drop(data);}
static const xnettaskownershipv1 task_policy={sizeof(task_policy),task,release,&root_ops};
static const xnettimerownershipv1 timer_policy={sizeof(timer_policy),timer,release,&root_ops};
static const xnettimerownershipv1 wrong_timer={sizeof(wrong_timer),timer,release,&root_ops};
static bool resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** output,
    const xrtownershippreparationv1** preparation)
{
    const xnettaskownershipv1* task_admission=&task_policy;
    const xnettimerownershipv1* timer_admission=*(bool*)data?&wrong_timer:&timer_policy;
    *preparation=NULL;
    *output=ref.Ops==&root_ops?&root_adapter:xrtNetBufPoolOwnershipAdapterV1(ref);
    if(!*output)*output=xrtNetEngineOwnershipAdapterV1(ref,&task_admission,1,&timer_admission,1,preparation);
    return *output!=NULL;
}
static XL6_OwnershipPlanResult create_plan(Root* root,bool* rejected,XL6_OwnershipPlan** plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV2(root_ref(root),resolve,rejected,plan);
        if(status>XL6_OWNERSHIP_PLAN_BUSY||(*rejected&&status==XL6_OWNERSHIP_PLAN_ERROR))return status;
        assert(!xl6DeadlineExpiredV1(deadline));xrtClearError();++retries;xrtThreadYield();}
}
static void destroy_plan(XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(xl6OwnershipPlanTryDestroy(plan)!=XL6_OWNERSHIP_PLAN_READY){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static XL6_OwnershipPlanResult settle(Root* root,XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        XL6_OwnershipPlanResult state=xl6OwnershipPlanTryRefresh(plan);
        if(state==XL6_OWNERSHIP_PLAN_ROOTED||state==XL6_OWNERSHIP_PLAN_FINALIZERS||state==XL6_OWNERSHIP_PLAN_READY)return state;
        if(xl6DeadlineExpiredV1(deadline)){fprintf(stderr,"Engine settle timeout: step=%d state=%d tasks=%u timers=%u active=%u\n",
            (int)step,(int)state,root->tasks,root->timers,root->active);abort();}
        xrtClearError();++retries;xrtThreadYield();}
}
static void one(unsigned mode)
{
    xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
    Root* root=xrtCalloc(1,sizeof(*root));assert(root);root->refs=1;root->resurrect=(mode&2)!=0;root->embedded=(mode&8)!=0;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;
    root->engine=xrtNetEngineCreate(&config);assert(root->engine&&xrtNetEngineStart(root->engine));
    xrtownershipscope freeze={0};idle_freeze(root->engine,&freeze);
    if(root->embedded){assert(xrtNetPostInit(&root->post)&&root_hold(root)&&
        xrtNetPostOwnedV1(xrtNetEngineWorker(root->engine,0),&root->post,root,&task_policy));++embedded_plans;}
    else assert(root_hold(root)&&xrtNetEnginePostOwnedV1(root->engine,0,root,&task_policy));
    assert(root_hold(root)&&xrtNetEngineScheduleOwnedV1(root->engine,0,xl6DeadlineAfterMillisecondsV1(3600000000ULL),root,&timer_policy));
    root_drop(root);
    xrtownershipref ref=root_ref(root);xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==3&&graph.EdgeCount==4&&graph.ExternalRootCount==0);
    end(&freeze);
    if(mode&4){
        XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
        for(;;){idle_freeze(root->engine,&freeze);
            if(root->tasks==1){assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&
                graph.NodeCount==3&&graph.EdgeCount==3&&graph.ExternalRootCount==0);end(&freeze);break;}
            end(&freeze);assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
        ++heap_plans;
    }
    bool rejected=(mode&1)!=0;XL6_OwnershipPlan* plan=NULL;
    if(rejected){assert(create_plan(root,&rejected,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan);++refusals;xrtClearError();rejected=false;}
    (void)create_plan(root,&rejected,&plan);assert(plan);XL6_OwnershipPlanResult state=settle(root,plan);
    if(root->resurrect){assert(state==XL6_OWNERSHIP_PLAN_ROOTED&&escaped==root&&!root->finalized);
        destroy_plan(plan);plan=NULL;
        /* The native callback edges have actually retired. Keep the escape as
         * a real external owner until a new ROOTED plan has acquired its pin;
         * dropping it then requires fresh reachability, not an RC adjustment. */
        assert(root->refs==1);++resurrections;
        /* Keep that real owner while obtaining a new plan; dropping it after
         * the plan pins makes the graph unreachable without a fake RC offset. */
        (void)create_plan(root,&rejected,&plan);assert(plan);root_drop(escaped);escaped=NULL;
        state=settle(root,plan);
    }
    assert(state==XL6_OWNERSHIP_PLAN_FINALIZERS&&!root->finalized);
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){XL6_OwnershipStepResult step=xl6OwnershipPlanFinalizeV2(plan);assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        state=xl6OwnershipPlanTryCommit(plan);if(state==XL6_OWNERSHIP_PLAN_READY)break;
        assert(state==XL6_OWNERSHIP_PLAN_BUSY||state==XL6_OWNERSHIP_PLAN_FINALIZERS);
        assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    assert(root->finalized&&root->tasks==1&&root->timers==1);
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
    for(unsigned i=0;i<12;++i)for(unsigned mode=0;mode<16;++mode)one(mode);
    assert(cycles==192&&tasks==192&&timers==192&&finalizers==192&&freed==192&&refusals==96&&resurrections==96&&heap_plans==96&&embedded_plans==96);
    assert(xrtMemDebugReset());
    printf("Engine real XLIR plans: cycles=%u tasks=%u timers=%u finalizers=%u freed=%u policy-refusals=%u resurrections=%u heap-plans=%u embedded-plans=%u retries=%u; exact memory; not transport/module/unload proof\n",
        cycles,tasks,timers,finalizers,freed,refusals,resurrections,heap_plans,embedded_plans,retries);return 0;
}
