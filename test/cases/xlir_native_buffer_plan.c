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
#include <string.h>

/* Unique buffers project slots from their actual counted host. A worker pool
 * is a real leaf, never a reverse owner of its buffers or the Engine. */
typedef struct Root {
    volatile int32 refs;
    xnetengine* engine;
    xnetbuf left, right;
    xnetbuf* external; /* borrowed setup destination, cleared by the worker */
    xnetbufpool* pool; /* borrowed through the two actual allocator slots */
    const void* claim;
    unsigned active, releases;
    bool ready, cleared, finalized, wrong, move, resurrect, engine_first;
} Root;
static Root* escaped;
static unsigned cycles, finalizers, freed, releases, refusals, resurrections, survivors, probes, retries, admission_retries;
static const xnetbufownershipv1 buffer_policy, wrong_policy;
static const char external_data[]="ab";
static void begin(xrtownershipscope* s){assert(xrtOwnershipMutationBegin(s));}
static void end(xrtownershipscope* s){assert(xrtOwnershipScopeEnd(s));}
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
    if(!refs){assert(!root->engine&&!root->active&&!root->external&&!root->pool&&
        !root->left.Pool&&!root->right.Pool);xrtFree(root);++freed;}
    end(&s);
}
static bool root_count(const void* data,size_t* count)
{
    const Root* root=data;if(root->cleared||root->active||root->refs<=0)return false;
    *count=(size_t)root->refs;return true;
}
static bool root_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const Root* root=data;const xnetbufownershipv1* admitted=root->wrong?&wrong_policy:&buffer_policy;
    return xrtNetBufTraceOwnershipV1(&root->left,&admitted,1,visit,context)&&
        xrtNetBufTraceOwnershipV1(&root->right,&admitted,1,visit,context)&&
        visit(xrtNetEngineOwnership(root->engine),context);
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
static bool count_visits(xrtownershipref ref,ptr data)
{if(ref.Data)++*(unsigned*)data;return true;}
static int32 probe_thread(ptr data)
{
    Root* root=data;xrtownershipscope s={0};freeze_begin(&s);size_t count=731;unsigned visited=0;
    const xnetbufownershipv1* admitted=&buffer_policy;
    assert(!root_count(root,&count)&&count==731);
    assert(!xrtNetBufTraceOwnershipV1(root->move?&root->right:&root->left,&admitted,1,count_visits,&visited)&&!visited);
    xrtownershipref ref=xrtNetBufPoolOwnership(root->pool);
    assert(!ref.Ops->Count(ref.Data,&count)&&count==731);end(&s);return 0;
}
static void buffer_release(ptr data,cbytes bytes,size_t size)
{
    Root* root=data;assert(bytes==(cbytes)external_data&&size==2&&root->active&&root->finalized&&!root->cleared);
    /* A callback is semantic work, outside mutation and before graph clear. */
    xthread* thread=xrtThreadCreate(probe_thread,root,0);assert(thread);
    assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);++probes;
    if(root->resurrect){assert(!escaped&&root_hold(root));escaped=root;}
    ++root->releases;++releases;root_drop(root);
}
static const xnetbufownershipv1 buffer_policy={sizeof(buffer_policy),buffer_release,&root_ops};
static const xnetbufownershipv1 wrong_policy={sizeof(wrong_policy),buffer_release,&root_ops};
static bool root_finalize(const void* data,const void* token)
{
    Root* root=(Root*)data;assert(root->claim==token&&!root->active&&!root->cleared);
    /* Aborting a resurrected plan must not replay semantic finalization. */
    if(root->finalized)return true;
    assert(root->ready&&xrtNetEngineState(root->engine)==XNET_ENGINE_DESTROYING);
    xnetbuf* buffer=root->move?&root->right:&root->left;char text[4];xnetbufpoolinfo info;
    xrtNetBufPoolGet(root->pool,&info);assert(info.LiveBlocks>=2);
    assert(xrtNetBufSize(buffer)==4&&xrtNetBufPeek(buffer,0,text,4)==4&&!memcmp(text,"abcd",4));
    xrtownershipscope s={0};begin(&s);++root->active;root->finalized=true;++finalizers;end(&s);
    xrtNetBufClear(&root->left);xrtNetBufClear(&root->right);
    begin(&s);--root->active;assert(root->releases==1);end(&s);return true;
}
static void root_clear(const void* data,const void* token)
{Root* root=(Root*)data;assert(root->claim==token&&root->finalized&&!root->cleared);root->cleared=true;}
static bool root_finish(const void* data,const void* token)
{
    Root* root=(Root*)data;xrtownershipscope s={0};begin(&s);assert(root->claim==token&&root->cleared&&root->releases==1);
    xnetengine* engine=root->engine;root->engine=NULL;root->pool=NULL;end(&s);
    if(root->engine_first)assert(xrtNetEngineTryDestroy(engine)==XNET_RETIRE_READY);
    xrtNetBufUnit(&root->left);xrtNetBufUnit(&root->right);
    if(!root->engine_first)assert(xrtNetEngineTryDestroy(engine)==XNET_RETIRE_READY);
    assert(root->releases==1);return true;
}
static const xrtownershipadapterv1 root_adapter={sizeof(root_adapter),root_hold,root_drop,
    root_claim,root_restore,root_finalize,root_clear,root_finish};
static void setup_task(xnetworker* worker,ptr data)
{
    Root* root=data;xrtownershipscope s={0};begin(&s);++root->active;
    assert(xrtNetWorkerEngine(worker)==root->engine);root->pool=xrtNetWorkerBufPool(worker);assert(root->pool);
    assert(xrtNetBufInitOwnedPoolV1(&root->left,root->pool)&&xrtNetBufInitOwnedPoolV1(&root->right,root->pool));
    assert(root_hold(root)&&xrtNetBufAppendOwnedV1(&root->left,external_data,2,root,&buffer_policy));
    assert(xrtNetBufAppend(&root->left,"cd",2));
    if(root->move)assert(xrtNetBufMove(&root->right,&root->left));
    if(root->external){assert(xrtNetBufInitOwnedPoolV1(root->external,root->pool)&&xrtNetBufAppend(root->external,"tail",4));root->external=NULL;}
    root->ready=true;--root->active;end(&s);
}
static void task_release(ptr data){root_drop(data);}
static const xnettaskownershipv1 task_policy={sizeof(task_policy),setup_task,task_release,&root_ops};
static bool resolve(xrtownershipref ref,void* data,const XL6_OwnershipAdapterV1** output,
    const xrtownershippreparationv1** preparation)
{
    (void)data;const xnettaskownershipv1* admitted=&task_policy;*preparation=NULL;
    *output=ref.Ops==&root_ops?&root_adapter:xrtNetBufPoolOwnershipAdapterV1(ref);
    if(!*output)*output=xrtNetEngineOwnershipAdapterV1(ref,&admitted,1,NULL,0,preparation);
    return *output!=NULL;
}
static XL6_OwnershipPlanResult create_plan(Root* root,XL6_OwnershipPlan** plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCreateV2(root_ref(root),resolve,NULL,plan);
        if(state>XL6_OWNERSHIP_PLAN_BUSY||(root->wrong&&state==XL6_OWNERSHIP_PLAN_ERROR))return state;
        assert(!xl6DeadlineExpiredV1(deadline));xrtClearError();++retries;xrtThreadYield();}
}
static void destroy_plan(XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(xl6OwnershipPlanTryDestroy(plan)!=XL6_OWNERSHIP_PLAN_READY){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static XL6_OwnershipPlanResult settle(XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        /* Prepare may initiate worker retirement before the worker has parked.
         * STEP_ERROR means the next snapshot refused admission, not that a
         * semantic Prepare callback failed. Keep the SAME plan and its pins. */
        if(step==XL6_OWNERSHIP_STEP_ERROR){assert(!xl6DeadlineExpiredV1(deadline));
            ++admission_retries;xrtClearError();xrtThreadYield();continue;}
        XL6_OwnershipPlanResult state=xl6OwnershipPlanTryRefresh(plan);
        if(state==XL6_OWNERSHIP_PLAN_ROOTED||state==XL6_OWNERSHIP_PLAN_FINALIZERS||state==XL6_OWNERSHIP_PLAN_READY)return state;
        assert(!xl6DeadlineExpiredV1(deadline));if(state==XL6_OWNERSHIP_PLAN_ERROR)++admission_retries;
        xrtClearError();++retries;xrtThreadYield();}
}
static XL6_OwnershipPlanResult finalize(XL6_OwnershipPlan* plan)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){assert(xl6OwnershipPlanFinalizeV2(plan)>=XL6_OWNERSHIP_STEP_BUSY);
        XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCommit(plan);
        if(state==XL6_OWNERSHIP_PLAN_READY||state==XL6_OWNERSHIP_PLAN_ROOTED)return state;
        assert((state==XL6_OWNERSHIP_PLAN_BUSY||state==XL6_OWNERSHIP_PLAN_FINALIZERS)&&!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void one(unsigned mode)
{
    xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
    Root* root=xrtCalloc(1,sizeof(*root));assert(root);root->refs=1;root->move=(mode&1)!=0;
    root->resurrect=(mode&2)!=0;root->engine_first=(mode&8)!=0;
    xnetbuf external={0};bool has_external=(mode&4)!=0;root->external=has_external?&external:NULL;
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;
    root->engine=xrtNetEngineCreate(&config);assert(root->engine&&xrtNetEngineStart(root->engine));
    assert(root_hold(root)&&xrtNetEnginePostOwnedV1(root->engine,0,root,&task_policy));
    xrtownershipscope freeze={0};XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){idle_freeze(root->engine,&freeze);if(root->ready)break;
        end(&freeze);assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    assert(root->refs==2);root_drop(root);
    xrtownershipref ref=root_ref(root);xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==3&&graph.EdgeCount==7&&
        graph.ExternalRootCount==(has_external?1u:0u));end(&freeze);
    XL6_OwnershipPlan* plan=NULL;root->wrong=true;
    assert(create_plan(root,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan&&!root->releases&&!root->finalized&&!root->claim);
    ++refusals;xrtClearError();root->wrong=false;
    (void)create_plan(root,&plan);assert(plan&&settle(plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    /* A rooted pool has no reverse path to Root/Engine. It must not prevent
     * their collection, but must survive it with its independent bytes. */
    XL6_OwnershipPlanResult state=finalize(plan);
    if(root->resurrect){assert(state==XL6_OWNERSHIP_PLAN_ROOTED&&escaped==root&&root->finalized&&!root->cleared);
        destroy_plan(plan);plan=NULL;assert(root->refs==1&&root->releases==1);++resurrections;
        assert(create_plan(root,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
        root_drop(escaped);escaped=NULL;assert(settle(plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);state=finalize(plan);
    }
    assert(state==XL6_OWNERSHIP_PLAN_READY&&root->cleared&&root->finalized&&root->releases==1);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY&&xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    destroy_plan(plan);
    if(has_external){char text[4];assert(xrtNetBufSize(&external)==4&&xrtNetBufPeek(&external,0,text,4)==4&&!memcmp(text,"tail",4));
        assert(!xrtNetBufPrepend(&external,"x",1)&&xrtErrorKind(xrtGetError())==XERR_CLOSED);xrtClearError();
        freeze_begin(&freeze);size_t count=0;ref=xrtNetBufPoolOwnership(external.Pool);
        assert(ref.Ops->Count(ref.Data,&count)&&count==2);end(&freeze);xrtNetBufUnit(&external);++survivors;
    }
    xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);++cycles;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned i=0;i<6;++i)for(unsigned mode=0;mode<16;++mode)one(mode);
    assert(cycles==96&&finalizers==96&&freed==96&&releases==96&&refusals==96&&resurrections==48&&survivors==48&&probes==96);
    assert(xrtMemDebugReset());
    printf("Buffer real XLIR plans: cycles=%u finalizers=%u freed=%u releases=%u policy-refusals=%u resurrections=%u pool-survivors=%u callback-probes=%u retries=%u admission-retries=%u; exact memory; not transport/module/unload proof\n",
        cycles,finalizers,freed,releases,refusals,resurrections,survivors,probes,retries,admission_retries);return 0;
}
