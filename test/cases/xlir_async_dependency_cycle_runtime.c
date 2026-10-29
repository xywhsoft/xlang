#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD
#define XRT_MODULE_VALUE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_future.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_future_payload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Owner {size_t references;XL6_ContinuationQueue *queue,*retiring;const void* claim;bool cleared;XL6_CodeOwnerGraph code;} Owner;
static Owner* owner;static xfuture* outputs[3];static unsigned width,caught[3],finalized[3],after[3],completed,refusals,ooms,preparations;
static xfuture* terminal;static bool linearChain;static unsigned loopStart,readyChains,feeders,cyclicInterruptions;
static int64_t payload=19;
static bool owner_count(const void* data,size_t* count)
{
    const Owner* value=data;if(!value||!count||value->cleared||!value->references)return false;
    *count=value->references;return true;
}
static bool owner_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const Owner* value=data;size_t count;
    return visit&&owner_count(data,&count)&&(!value->queue||visit(xl6ContinuationQueueOwnership(value->queue),context));
}
static const xrtownershipops owner_ops={owner_count,owner_trace};
static bool owner_hold(const void* data)
{
    Owner* value=(Owner*)data;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    assert(value->references&&value->references<SIZE_MAX&&!value->cleared);++value->references;
    assert(xrtOwnershipScopeEnd(&mutation));return true;
}
static void owner_drop(const void* data)
{
    Owner* value=(Owner*)data;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation)&&value->references);
    if(!--value->references){assert(value->cleared&&!value->queue&&!value->retiring);xrtFree(value);}
    assert(xrtOwnershipScopeEnd(&mutation));
}
static int code_acquire(void* data) {return owner_hold(data)?1:0;}
static void code_release(void* data) {owner_drop(data);}
static bool owner_claim(const void* data,const void* token)
{
    Owner* value=(Owner*)data;if(!token||value->cleared||(value->claim&&value->claim!=token))return false;
    value->claim=token;return true;
}
static void owner_restore(const void* data,const void* token)
{Owner* value=(Owner*)data;assert(token&&value->claim==token&&!value->cleared);value->claim=NULL;}
static void owner_clear(const void* data,const void* token)
{
    Owner* value=(Owner*)data;assert(token&&value->claim==token&&!value->cleared&&!value->retiring);
    value->cleared=true;value->retiring=value->queue;value->queue=NULL;
}
static bool owner_finish(const void* data,const void* token)
{
    Owner* value=(Owner*)data;assert(token&&value->claim==token&&value->cleared&&value->retiring);
    assert(xl6ContinuationQueueDestroy(value->retiring));value->retiring=NULL;return true;
}
static const xrtownershipadapterv1 owner_adapter={sizeof(owner_adapter),owner_hold,owner_drop,owner_claim,owner_restore,NULL,owner_clear,owner_finish};
typedef struct Resolve {bool omitQueue,omitStorage,omitFrame,omitWatch;} Resolve;
static bool resolve(xrtownershipref ref,void* data,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation)
{
    Resolve* mode=data;*adapter=NULL;*preparation=NULL;
    if(ref.Ops==&owner_ops){*adapter=&owner_adapter;return true;}
    const XL6_ContinuationStorageV2* storage[]={xl6ContinuationProducedStorageV2Get()};
    if(!mode||!mode->omitQueue){
        *adapter=xl6ContinuationQueueOwnershipAdapterV1(ref,storage,mode&&mode->omitStorage?0:1,preparation);
        if(*adapter)return true;
    }
    if(!mode||!mode->omitFrame){
        *adapter=xl6ContinuationFrameOwnershipAdapterV1(ref,preparation);if(*adapter)return true;
    }
    const xfutureproducerownershipv1* producers[]={xl6ContinuationProducerPolicyV1Get()};
    const xfuturewatchownershipv1* watches[]={xl6ContinuationWatchPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[XL6_FUTURE_PAYLOAD_COUNT-XL6_FUTURE_PAYLOAD_VALUE];
    for(unsigned i=0;i<sizeof(payloads)/sizeof(*payloads);++i)payloads[i]=xl6PayloadPolicyGet((XL6_FuturePayloadKind)(XL6_FUTURE_PAYLOAD_VALUE+i));
    *adapter=xrtFutureOwnershipAdapterV3(ref,payloads,sizeof(payloads)/sizeof(*payloads),producers,1,watches,mode&&mode->omitWatch?0:1,preparation);
    if(*adapter)return true;
    if((*adapter=xl6CodeLeaseOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtCancelOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtErrorOwnershipAdapterV1(ref)))return true;
    *adapter=xrtValueOwnershipAdapterV1(ref);return *adapter!=NULL;
}
XL6_ContinuationQueue* dependency_queue(void) {return owner->queue;}
const XL6_CodeOwner* dependency_owner(void) {return &owner->code.base;}
xfuture* dependency_peer(int64_t id)
{
    assert(id>=0&&(unsigned)id<width);
    return xrtFutureRef(linearChain&&(unsigned)id==width-1?terminal:outputs[(unsigned)id==width-1?loopStart:(unsigned)id+1]);
}
void dependency_caught(int64_t id) {assert(id>=0&&(unsigned)id<width&&!caught[id]);++caught[id];}
void dependency_finally(int64_t id) {assert(id>=0&&(unsigned)id<width&&!finalized[id]);++finalized[id];}
void dependency_after(int64_t id) {assert(id>=0&&(unsigned)id<width);++after[id];}
xfuture* dependency_boot(void) {assert(!"reachability-only entry");return NULL;}
extern xfuture* dependency_start(unsigned,xfuture*,xfuture*,bool);
extern void dependency_prepare(void),dependency_finish(void);
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    assert(now.LiveCount==before->LiveCount&&now.LiveBytes==before->LiveBytes&&
        now.AllocCount-before->AllocCount==now.FreeCount-before->FreeCount&&
        now.InvalidFreeCount==before->InvalidFreeCount&&now.DoubleFreeCount==before->DoubleFreeCount&&now.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void cycle(unsigned size,bool waitCleanup,unsigned rootKind,bool allocationSweep,bool linear,bool feeder)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);width=size;linearChain=linear;loopStart=feeder?1u:0u;
    memset(outputs,0,sizeof(outputs));memset(caught,0,sizeof(caught));memset(finalized,0,sizeof(finalized));memset(after,0,sizeof(after));
    owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->references=1;owner->queue=xl6ContinuationQueueCreate(size);assert(owner->queue);
    owner->code=(XL6_CodeOwnerGraph){{sizeof(XL6_CodeOwnerGraph),owner,code_acquire,code_release},0x584C36434F574E31ULL,&owner_ops};
    xfuture *boot=NULL,*cleanup=NULL;xpromise* bootPromise=xrtPromiseCreate(&boot,NULL);
    xpromise* cleanupPromise=xrtPromiseCreate(&cleanup,NULL);assert(bootPromise&&cleanupPromise);
    terminal=cleanup;
    if(!waitCleanup)assert(xrtPromiseResolve(cleanupPromise,&payload));
    for(unsigned i=0;i<size;++i)outputs[i]=dependency_start(i,boot,cleanup,waitCleanup);
    /* Entry is now an admitted ready activation. Reach the initial pending
     * boot await before exercising the original boot-to-peer transition. */
    for(unsigned i=0;i<size;++i)assert(xl6ContinuationRunOne(owner->queue));
    assert(!xl6ContinuationRunOne(owner->queue));
    assert(xrtPromiseResolve(bootPromise,&payload));
    for(unsigned i=0;i<size;++i)assert(xl6ContinuationRunOne(owner->queue));
    if(!linear)assert(!xl6ContinuationRunOne(owner->queue));
    assert(xl6ContinuationQueueActivationCount(owner->queue)==size);
    xrtPromiseDestroy(bootPromise);xrtFutureDestroy(boot);
    xrtownershipref root={owner,&owner_ops};Resolve denied[4]={{true,false,false,false},{false,true,false,false},{false,false,true,false},{false,false,false,true}};
    for(unsigned i=0;i<4;++i){XL6_OwnershipPlan* failed=NULL;xmemdebugsnapshot checkpoint;xrtMemDebugSnapshot(&checkpoint);
        assert(xl6OwnershipPlanTryCreateV2(root,resolve,&denied[i],&failed)==XL6_OWNERSHIP_PLAN_ERROR&&!failed);xrtClearError();
        balance(&checkpoint);++refusals;
    }
    if(allocationSweep)for(unsigned budget=0;budget<80;++budget){
        XL6_OwnershipPlan* probe=NULL;xmemdebugsnapshot checkpoint;xrtMemDebugSnapshot(&checkpoint);assert(xrtMemDebugFailAfter(budget));
        XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&probe);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(probe){assert(status==XL6_OWNERSHIP_PLAN_ROOTED);assert(xl6OwnershipPlanTryDestroy(probe)==XL6_OWNERSHIP_PLAN_READY);}
        else {assert(status==XL6_OWNERSHIP_PLAN_ERROR&&failed);++ooms;}
        balance(&checkpoint);
    }
    XL6_OwnershipPlan* plan=NULL;assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    /* One observation OR the host owner reference is enough to forbid any
     * cancellation. Queue Close alone also does not confer a graph claim. */
    if(rootKind==0)owner_drop(owner);
    for(unsigned i=rootKind==0?1:0;i<size;++i)xrtFutureDestroy(outputs[i]);
    assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    if(!linear)assert(!xl6ContinuationRunOne(owner->queue));
    for(unsigned i=0;i<size;++i)assert(!xrtFutureDone(outputs[i])&&!caught[i]&&!finalized[i]);
    if(rootKind==0)xrtFutureDestroy(outputs[0]);else owner_drop(owner);
    assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
    XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);
    assert(step==XL6_OWNERSHIP_STEP_BUSY);++preparations;
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
    unsigned requests=0;
    for(unsigned i=0;i<size;++i){xcancel* cancel=xrtFutureCancelToken(outputs[i]);assert(cancel);
        if(feeder&&i==0)assert(!xrtCancelRequested(cancel));
        if(xrtCancelRequested(cancel))++requests;
        xrtCancelDestroy(cancel);}
    /* Unreachability alone is not permission to cancel accepted work. Only
     * an actual wait SCC needs interruption, and one member breaks this ring. */
    assert(requests==(linear?0u:1u));cyclicInterruptions+=requests;
    for(unsigned i=0;i<size;++i)assert(!xrtFutureDone(outputs[i])&&!caught[i]&&!finalized[i]);
    while(xl6ContinuationRunOne(owner->queue)){}
    if(waitCleanup){
        unsigned entered=0;
        for(unsigned i=0;i<size;++i){entered+=caught[i];assert(!finalized[i]&&!xrtFutureDone(outputs[i]));}
        assert(entered==1); /* peers wait for the interrupted member's finally */
        /* The newly awaited external cleanup is a real root. Do not drop the
         * plan, replay cancellation, or finalize an unfinished activation. */
        assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY&&!xl6ContinuationRunOne(owner->queue));
        assert(xrtPromiseResolve(cleanupPromise,&payload));while(xl6ContinuationRunOne(owner->queue)){}
    }
    for(unsigned i=0;i<size;++i){
        assert(caught[i]==(linear?0u:1u)&&finalized[i]==1&&after[i]==(linear?1u:0u));
        assert(xrtFutureState(outputs[i])==(linear?XFUTURE_RESOLVED:XFUTURE_CANCELLED));
        if(linear)assert(*(int64_t*)xrtFutureValue(outputs[i])==payload);
    }
    assert(xl6ContinuationQueueIsIdle(owner->queue));
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    /* TryCommit may lose the global freeze to an unrelated worker's physical
     * transition. It invokes no language cleanup on BUSY; all work above is
     * already terminal. Require the actual commit before finishing the plan. */
    xdeadline commitDeadline=xrtDeadlineAfter(5000000);
    for(;;){
        XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCommit(plan);
        if(result==XL6_OWNERSHIP_PLAN_READY)break;
        assert(result==XL6_OWNERSHIP_PLAN_BUSY&&!xrtGetError()&&!xrtDeadlineExpired(commitDeadline));
        xrtThreadYield();
    }
    assert(xl6OwnershipPlanFinish(plan)&&xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);owner=NULL;
    xrtPromiseDestroy(cleanupPromise);xrtFutureDestroy(cleanup);terminal=NULL;assert(!xrtGetError());balance(&before);++completed;
    if(linear)++readyChains;
    if(feeder)++feeders;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);dependency_prepare();
    for(unsigned round=0;round<10;++round)for(unsigned size=2;size<=3;++size)
    for(unsigned root=0;root<2;++root)cycle(size,false,root,false,true,false);
    for(unsigned round=0;round<10;++round)for(unsigned size=1;size<=3;++size)
    for(unsigned cleanup=0;cleanup<2;++cleanup)for(unsigned root=0;root<2;++root)cycle(size,cleanup!=0,root,!cleanup&&!root,false,false);
    for(unsigned round=0;round<10;++round)for(unsigned cleanup=0;cleanup<2;++cleanup)
    for(unsigned root=0;root<2;++root)cycle(3,cleanup!=0,root,false,false,true);
    assert(completed==200&&readyChains==40&&feeders==40&&refusals==800&&preparations==200&&cyclicInterruptions==160&&ooms>20);
    dependency_finish();balance(&before);
    printf("Async dependency cycles: %u generated one/two/three-producer rings, %u runnable acyclic chains, %u cycle feeders, %u exact queue/storage/frame/Watch refusals, %u authorized cyclic interruptions, %u real snapshot OOM failures; external roots preserved, ready work and feeders not cancelled, normal catch/finally and cleanup-await resurrection, exact queue/frame/owner retirement and memory balance\n",completed-readyChains-feeders,readyChains,feeders,refusals,cyclicInterruptions,ooms);
    return 0;
}
