#include "release/include/xl_clock.h"
#include "net_task_owned_test_clock.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_task_net.h"
#include "src/runtime/xl6_closure.h"
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
    TestDeadline deadline=testDeadline();
    while(!xrtOwnershipFreezeTryBegin(s)){assert(!testExpired(deadline));xrtThreadYield();}
}
static void idle_freeze(xnetengine* engine,xrtownershipscope* s)
{
    xrtownershipref ref=xrtNetEngineOwnership(engine);size_t count;TestDeadline deadline=testDeadline();
    for(;;){freeze_begin(s);if(ref.Ops->Count(ref.Data,&count))return;
        end(s);assert(!testExpired(deadline));xrtThreadYield();}
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
    Root* root=(Root*)data;assert(root->claim==token&&!root->active&&!root->cleared);
    /* Parent/closure finalization order is not a native Data-drop barrier.
     * A resurrected already-finalized object must not run user finalizers twice. */
    if(root->finalized)return true;
    xfuturestate expected=root->mode==2?XFUTURE_CANCELLED:root->mode==1?XFUTURE_CLOSED:
        root->mode==3?XFUTURE_FAILED:XFUTURE_RESOLVED;
    assert(xrtFutureState(root->future)==expected);
    assert(root->runs==((root->mode==1||root->mode==2)?0u:1u));
    assert(xrtNetEngineState(root->engine)==XNET_ENGINE_DESTROYING);
    root->finalized=true;++finalizers;
    /* Language-visible resurrection belongs to semantic Finalize, never the
     * later resident physical environment-reference release in Finish. */
    if(root->resurrect){assert(!escaped&&root_hold(root));escaped=root;}
    return true;
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
    assert(!unused&&root->drops==0);
    ++root->drops;++data_drops;
    --root->active;end(&s);root_drop(data);
}
typedef struct Work {XL6_Closure* closure;} Work;
static const unsigned environmentSchema=1;
static XL6_OwnershipCatalog* catalog;
static xtasknetdataownershipv1 wrong_policy;
static bool environmentValid(const void* schema,const void* data,size_t size)
{return schema==&environmentSchema&&data&&size==sizeof(Root);}
static bool environmentTrace(const void* schema,const void* data,size_t size,XL6_ModuleOwnershipVisit visit,void* context)
{return environmentValid(schema,data,size)&&visit(root_ref(data),context);}
static bool environmentDrop(const void* schema,void* data,size_t size)
{assert(environmentValid(schema,data,size));data_drop(data,NULL);return true;}
static const XL6_ClosureEnvironmentOpsV1 environmentOps={
    sizeof(environmentOps),environmentValid,environmentTrace,environmentDrop
};
static XL6_TaskOutcome core_run(xcancel* cancel,void* raw,XL6_TaskResult* result)
{
    Work* work=raw;Root* root=work->closure->environment;xtaskvalue value={0};
    xtaskoutcome outcome=run(xrtNetEngineCurrent(root->engine),cancel,root,&value);
    *result=(XL6_TaskResult){value.Value,value.Destroy,value.DestroyData};
    return outcome==XTASK_SUCCESS?XL6_TASK_SUCCESS:outcome==XTASK_CANCELLED?XL6_TASK_CANCELLED:XL6_TASK_FAILED;
}
static const XL6_CaptureSlotV2 captureSlots[]={{{XL6_CAPTURE_CLOSURE,offsetof(Work,closure),sizeof(XL6_Closure*)},NULL}};
static const XL6_CaptureLayoutV2 captureLayout={sizeof(captureLayout),sizeof(Work),1,captureSlots};
static bool closure_marker(void* environment){return environment!=NULL;}
static Work* create_work(Root* root)
{
    Work* work=xl6TaskRuntimeV3Get()->Allocate(NULL,sizeof(*work),core_run,&captureLayout);assert(work);
    work->closure=xl6ClosureCreateEnvironmentV1(NULL,(void (*)(void))closure_marker,NULL,root,sizeof(*root),
        &environmentSchema,&environmentOps,NULL,NULL,0);assert(work->closure);return work;
}
static bool load_root(XL6_OwnershipCatalog* c)
{return xl6OwnershipCatalogAddPayloadV1(c,&payload_policy);}
static XL6_OwnershipAdmissionV1 probe(xrtownershipref ref,void* data,
    const XL6_OwnershipAdapterV1** output,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    if(ref.Ops==&root_ops){*output=&root_adapter;return XL6_OWNERSHIP_ADMISSION_ADMITTED;}
    if(*(bool*)data&&ref.Ops==xrtTaskNetCancelPolicyV1Get()->Ops) {
        const xtasknetdataownershipv1* wrong=&wrong_policy;
        *output=xrtTaskNetOwnershipAdapterV1(ref,&wrong,1,preparation);
        assert(!*output);return XL6_OWNERSHIP_ADMISSION_REFUSED;
    }
    return xl6OwnershipCatalogProbeV1(ref,catalog,output,preparation,completion);
}
static XL6_OwnershipPlanResult create_plan(Root* root,bool* rejected,XL6_OwnershipPlan** plan)
{
    TestDeadline deadline=testDeadline();
    for(;;){XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCreateV6(root_ref(root),probe,NULL,false,rejected,plan);
        if(state>XL6_OWNERSHIP_PLAN_BUSY||(*rejected&&state==XL6_OWNERSHIP_PLAN_ERROR))return state;
        assert(!testExpired(deadline));xrtClearError();++retries;xrtThreadYield();}
}
static void destroy_plan(XL6_OwnershipPlan* plan)
{
    TestDeadline deadline=testDeadline();
    while(xl6OwnershipPlanTryDestroy(plan)!=XL6_OWNERSHIP_PLAN_READY){assert(!testExpired(deadline));xrtThreadYield();}
}
static XL6_OwnershipPlanResult settle(Root* root,XL6_OwnershipPlan* plan)
{
    TestDeadline deadline=testDeadline();
    for(;;){XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        XL6_OwnershipPlanResult state=xl6OwnershipPlanTryRefresh(plan);
        if(state==XL6_OWNERSHIP_PLAN_ROOTED||state==XL6_OWNERSHIP_PLAN_FINALIZERS||state==XL6_OWNERSHIP_PLAN_READY)return state;
        if(testExpired(deadline)){fprintf(stderr,"TaskNet settle timeout: mode=%u step=%d state=%d runs=%u drops=%u\n",
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
    Work* work=create_work(root);
    root->future=xl6TaskNetSubmitOwnedPolicyV1(root->engine,0,work,NULL,&payload_policy,delayed,
        xl6DeadlineAfterMillisecondsV1(mode==4?0:TEST_DELAY));
    assert(root->future);xrtownershipref ref=root_ref(root);xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==(delayed?12u:11u)&&graph.ExternalRootCount==1);
    if(!pending_graphs||pending_graphs==2)printf("Shared task graph: delayed=%u nodes=%zu edges=%zu\n",delayed,graph.NodeCount,graph.EdgeCount);
    ++pending_graphs;end(&freeze);
    /* A zero-deadline timer must actually execute successfully. Do not let
     * planner shutdown win first and count a CLOSED timer as this positive. */
    if(mode==4)assert(xrtFutureWaitFor(root->future,TEST_WAIT)==XWAIT_OK);
    bool rejected=mode==1||mode==2;XL6_OwnershipPlan* plan=NULL;
    if(rejected){assert(create_plan(root,&rejected,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan);++refusals;xrtClearError();rejected=false;}
    assert(create_plan(root,&rejected,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    if(mode==2)assert(xrtFutureCancel(root->future));
    root_drop(root);XL6_OwnershipPlanResult state=settle(root,plan);
    if(resurrect){
        /* Collector pins can keep the captured Closure's physical environment
         * reference alive through Finish, after capture-slot release. Drive
         * Root semantic Finalize and require a real external resurrection before
         * discarding/rebuilding the plan; never infer it from a reference delta. */
        TestDeadline resurrectionDeadline=testDeadline();
        while(!escaped){
            assert(state==XL6_OWNERSHIP_PLAN_FINALIZERS);
            assert(xl6OwnershipPlanFinalizeV2(plan)!=XL6_OWNERSHIP_STEP_FAILED);
            state=xl6OwnershipPlanTryCommit(plan);
            assert(state!=XL6_OWNERSHIP_PLAN_ERROR&&state!=XL6_OWNERSHIP_PLAN_READY);
            assert(!testExpired(resurrectionDeadline));
        }
        assert(state==XL6_OWNERSHIP_PLAN_ROOTED&&escaped==root);
        destroy_plan(plan);plan=NULL;++resurrections;
        assert(create_plan(root,&rejected,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
        root_drop(escaped);escaped=NULL;state=settle(root,plan);}
    assert(state==XL6_OWNERSHIP_PLAN_FINALIZERS&&(resurrect||!root->finalized));
    TestDeadline deadline=testDeadline();
    for(;;){XL6_OwnershipStepResult step=xl6OwnershipPlanFinalizeV2(plan);assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        state=xl6OwnershipPlanTryCommit(plan);if(state==XL6_OWNERSHIP_PLAN_READY)break;
        assert(state==XL6_OWNERSHIP_PLAN_BUSY||state==XL6_OWNERSHIP_PLAN_FINALIZERS);
        assert(!testExpired(deadline));xrtThreadYield();}
    assert(root->finalized);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY&&xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(root->drops==1);
    destroy_plan(plan);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);++cycles;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    wrong_policy=*xl6TaskNetDataPolicyV1Get();
    const XL6_OwnershipDomainV1 rootDomain={.size=sizeof(rootDomain),.Load=load_root};
    const XL6_OwnershipDomainV1* domains[]={&rootDomain,
#include "task-net-frame-domains.h"
    };
    catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    for(unsigned i=0;i<8;++i)for(unsigned mode=0;mode<5;++mode){one(mode,false);one(mode,true);}
    assert(cycles==80&&runs==48&&data_drops==80&&payload_drops==32&&finalizers==80&&freed==80&&
        refusals==32&&resurrections==40&&pending_graphs==80);
    xl6OwnershipCatalogDestroyV1(catalog);catalog=NULL;
    assert(xrtMemDebugReset());
    printf("Shared TaskNet real XLIR plans: cycles=%u runs=%u data-drops=%u payload-drops=%u finalizers=%u freed=%u refusals=%u resurrections=%u pending-graphs=%u retries=%u; exact memory; not full service/module/unload proof\n",
        cycles,runs,data_drops,payload_drops,finalizers,freed,refusals,resurrections,pending_graphs,retries);return 0;
}
