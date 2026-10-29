#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#define XRT_MODULE_FUTURE
#define XRT_MODULE_VALUE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_future.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_future_payload.h"
#include <assert.h>
#include <stdio.h>

typedef struct Owner {xatomic64 references;XL6_ContinuationExecutor *executor,*retiring;const void* claim;bool cleared;XL6_CodeOwnerGraph code;} Owner;
static Owner* owner;static xfuture* outputs[3];static unsigned width,loopStart;
static xatomic64 peers,caught[3],finalized[3],after[3];
static unsigned completed,refusals,resurrections;static int64_t payload=19;
static bool owner_count(const void* data,size_t* count)
{
    const Owner* value=data;if(!value||!count||value->cleared)return false;
    *count=(size_t)xrtAtomic64Load(&value->references,XMEMORY_ACQUIRE);return *count!=0;
}
static bool owner_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const Owner* value=data;size_t count;
    return visit&&owner_count(data,&count)&&(!value->executor||visit(xl6ContinuationExecutorOwnership(value->executor),context));
}
static const xrtownershipops owner_ops={owner_count,owner_trace};
static bool owner_hold(const void* data)
{
    Owner* value=(Owner*)data;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    assert(!value->cleared);uint64 prior=xrtAtomic64FetchAdd(&value->references,1,XMEMORY_ACQ_REL);assert(prior&&prior<SIZE_MAX);
    assert(xrtOwnershipScopeEnd(&mutation));return true;
}
static void owner_drop(const void* data)
{
    Owner* value=(Owner*)data;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    uint64 prior=xrtAtomic64FetchSub(&value->references,1,XMEMORY_ACQ_REL);assert(prior);
    if(prior==1){assert(value->cleared&&!value->executor&&!value->retiring);xrtFree(value);}
    assert(xrtOwnershipScopeEnd(&mutation));
}
static int code_acquire(void* data) {return owner_hold(data)?1:0;}
static void code_release(void* data) {owner_drop(data);}
static bool owner_claim(const void* data,const void* token)
{Owner* value=(Owner*)data;if(!token||value->cleared||(value->claim&&value->claim!=token))return false;value->claim=token;return true;}
static void owner_restore(const void* data,const void* token)
{Owner* value=(Owner*)data;assert(token&&value->claim==token&&!value->cleared);value->claim=NULL;}
static void owner_clear(const void* data,const void* token)
{
    Owner* value=(Owner*)data;assert(token&&value->claim==token&&!value->cleared&&!value->retiring);
    value->cleared=true;value->retiring=value->executor;value->executor=NULL;
}
static bool owner_finish(const void* data,const void* token)
{
    Owner* value=(Owner*)data;assert(token&&value->claim==token&&value->cleared&&value->retiring);
    assert(xl6ContinuationExecutorDestroy(value->retiring));value->retiring=NULL;return true;
}
static const xrtownershipadapterv1 owner_adapter={sizeof(owner_adapter),owner_hold,owner_drop,owner_claim,owner_restore,NULL,owner_clear,owner_finish};
typedef struct Resolve {unsigned omit;bool busy;xrtownershipref executor,queue;} Resolve;
static bool resolve(xrtownershipref ref,void* data,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation)
{
    Resolve* context=data;*adapter=NULL;*preparation=NULL;
    if(ref.Ops==&owner_ops){*adapter=&owner_adapter;return true;}
    if(ref.Ops==context->executor.Ops){
        if(context->omit==1)return false;
        *adapter=xl6ContinuationExecutorOwnershipAdapterV1(ref,preparation);if(!*adapter)context->busy=true;
        return *adapter!=NULL;
    }
    const XL6_ContinuationStorageV2* storage[]={xl6ContinuationProducedStorageV2Get()};
    if(context->omit!=2){
        *adapter=xl6ContinuationQueueOwnershipAdapterV1(ref,storage,1,preparation);
        if(*adapter){context->queue=ref;return true;}
        if(context->queue.Ops&&ref.Ops==context->queue.Ops){context->busy=true;return false;}
    }
    if(context->omit!=3){*adapter=xl6ContinuationFrameOwnershipAdapterV1(ref,preparation);if(*adapter)return true;}
    const xfutureproducerownershipv1* producers[]={xl6ContinuationProducerPolicyV1Get()};
    const xfuturewatchownershipv1* watches[]={xl6ContinuationWatchPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[XL6_FUTURE_PAYLOAD_COUNT-XL6_FUTURE_PAYLOAD_VALUE];
    for(unsigned i=0;i<sizeof(payloads)/sizeof(*payloads);++i)payloads[i]=xl6PayloadPolicyGet((XL6_FuturePayloadKind)(XL6_FUTURE_PAYLOAD_VALUE+i));
    *adapter=xrtFutureOwnershipAdapterV3(ref,payloads,sizeof(payloads)/sizeof(*payloads),producers,1,watches,context->omit==4?0:1,preparation);
    if(*adapter)return true;
    if((*adapter=xl6CodeLeaseOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtCancelOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtErrorOwnershipAdapterV1(ref)))return true;
    *adapter=xrtValueOwnershipAdapterV1(ref);return *adapter!=NULL;
}
XL6_ContinuationExecutor* dependency_executor(void) {return owner->executor;}
const XL6_CodeOwner* dependency_owner(void) {return &owner->code.base;}
xfuture* dependency_peer(int64_t id)
{
    assert(id>=0&&(unsigned)id<width);xrtAtomic64FetchAdd(&peers,1,XMEMORY_RELEASE);
    return xrtFutureRef(outputs[(unsigned)id==width-1?loopStart:(unsigned)id+1]);
}
void dependency_caught(int64_t id) {assert(id>=0&&(unsigned)id<width);assert(xrtAtomic64FetchAdd(&caught[id],1,XMEMORY_ACQ_REL)==0);}
void dependency_finally(int64_t id) {assert(id>=0&&(unsigned)id<width);assert(xrtAtomic64FetchAdd(&finalized[id],1,XMEMORY_ACQ_REL)==0);}
void dependency_after(int64_t id) {assert(id>=0&&(unsigned)id<width);xrtAtomic64FetchAdd(&after[id],1,XMEMORY_ACQ_REL);}
xfuture* dependency_boot(void) {assert(!"reachability-only entry");return NULL;}
extern xfuture* dependency_start(unsigned,xfuture*,xfuture*,bool);
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    assert(now.LiveCount==before->LiveCount&&now.LiveBytes==before->LiveBytes&&
        now.AllocCount-before->AllocCount==now.FreeCount-before->FreeCount&&
        now.InvalidFreeCount==before->InvalidFreeCount&&now.DoubleFreeCount==before->DoubleFreeCount&&now.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void quiet_freeze(xrtownershipref ref,xrtownershipscope* freeze)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){
        if(xrtOwnershipFreezeTryBegin(freeze)){
            size_t count;if(ref.Ops->Count(ref.Data,&count))return;
            assert(xrtOwnershipScopeEnd(freeze));
        }
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
static XL6_OwnershipPlanResult create_plan(xrtownershipref root,Resolve* context,XL6_OwnershipPlan** output)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){
        context->busy=false;XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreateV2(root,resolve,context,output);
        if(result!=XL6_OWNERSHIP_PLAN_BUSY&&!(result==XL6_OWNERSHIP_PLAN_ERROR&&context->busy))return result;
        assert(!*output);xrtClearError();assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
static void prepare_rooted(XL6_OwnershipPlan* plan,Resolve* context)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){
        context->busy=false;XL6_OwnershipPlanResult state=xl6OwnershipPlanTryRefresh(plan);
        if(state==XL6_OWNERSHIP_PLAN_ERROR){assert(context->busy);xrtClearError();}
        else if(state!=XL6_OWNERSHIP_PLAN_BUSY){
            assert(state==XL6_OWNERSHIP_PLAN_ROOTED);
            context->busy=false;XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);
            if(step==XL6_OWNERSHIP_STEP_READY)return;
            if(step==XL6_OWNERSHIP_STEP_ERROR){assert(context->busy);xrtClearError();}
            else assert(step==XL6_OWNERSHIP_STEP_BUSY);
        }
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
static void prepare_until(XL6_OwnershipPlan* plan,Resolve* context,bool allDone)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){
        context->busy=false;XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);
        if(step==XL6_OWNERSHIP_STEP_ERROR){assert(context->busy);xrtClearError();}
        else assert(step==XL6_OWNERSHIP_STEP_BUSY||step==XL6_OWNERSHIP_STEP_READY);
        unsigned done=0,entered=0;
        for(unsigned i=0;i<width;++i){done+=xrtFutureDone(outputs[i])?1u:0u;entered+=(unsigned)xrtAtomic64Load(&caught[i],XMEMORY_ACQUIRE);}
        if(allDone?done==width:entered==1)return;
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
static void cycle(unsigned size,bool waitCleanup,unsigned rootKind,bool feeder)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);width=size;loopStart=feeder?1u:0u;
    xrtAtomic64Store(&peers,0,XMEMORY_RELEASE);
    for(unsigned i=0;i<3;++i){outputs[i]=NULL;xrtAtomic64Store(&caught[i],0,XMEMORY_RELEASE);xrtAtomic64Store(&finalized[i],0,XMEMORY_RELEASE);xrtAtomic64Store(&after[i],0,XMEMORY_RELEASE);}
    owner=xrtCalloc(1,sizeof(*owner));assert(owner);xrtAtomic64Store(&owner->references,1,XMEMORY_RELEASE);
    owner->executor=xl6ContinuationExecutorCreateGuaranteed(size,1);assert(owner->executor);
    owner->code=(XL6_CodeOwnerGraph){{sizeof(XL6_CodeOwnerGraph),owner,code_acquire,code_release},0x584C36434F574E31ULL,&owner_ops};
    xfuture *boot=NULL,*cleanup=NULL;xpromise* bootPromise=xrtPromiseCreate(&boot,NULL);xpromise* cleanupPromise=xrtPromiseCreate(&cleanup,NULL);
    assert(bootPromise&&cleanupPromise);if(!waitCleanup)assert(xrtPromiseResolve(cleanupPromise,&payload));
    for(unsigned i=0;i<size;++i)outputs[i]=dependency_start(i,boot,cleanup,waitCleanup);
    assert(xrtPromiseResolve(bootPromise,&payload));xdeadline deadline=xrtDeadlineAfter(5000000);
    while(xrtAtomic64Load(&peers,XMEMORY_ACQUIRE)!=size){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    xrtownershipref executor=xl6ContinuationExecutorOwnership(owner->executor);xrtownershipscope freeze={0};quiet_freeze(executor,&freeze);
    assert(xrtOwnershipScopeEnd(&freeze));
    xrtPromiseDestroy(bootPromise);xrtFutureDestroy(boot);
    xrtownershipref root={owner,&owner_ops};Resolve context={0,false,executor,{0}};
    for(unsigned omit=1;omit<=4;++omit){
        context.omit=omit;XL6_OwnershipPlan* failed=NULL;xmemdebugsnapshot checkpoint;xrtMemDebugSnapshot(&checkpoint);
        assert(create_plan(root,&context,&failed)==XL6_OWNERSHIP_PLAN_ERROR&&!failed);xrtClearError();balanced(&checkpoint);++refusals;
    }
    context.omit=0;XL6_OwnershipPlan* plan=NULL;
    assert(create_plan(root,&context,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    if(!rootKind)owner_drop(owner);
    for(unsigned i=!rootKind?1u:0u;i<size;++i)xrtFutureDestroy(outputs[i]);
    prepare_rooted(plan,&context);
    for(unsigned i=0;i<size;++i)assert(!xrtFutureDone(outputs[i])&&!xrtAtomic64Load(&caught[i],XMEMORY_ACQUIRE));
    if(!rootKind)xrtFutureDestroy(outputs[0]);else owner_drop(owner);
    prepare_until(plan,&context,!waitCleanup);
    if(waitCleanup){
        quiet_freeze(executor,&freeze);unsigned requests=0;
        for(unsigned i=0;i<size;++i){
            assert(!xrtFutureDone(outputs[i])&&!xrtAtomic64Load(&finalized[i],XMEMORY_ACQUIRE));
            xcancel* cancel=xrtFutureCancelToken(outputs[i]);assert(cancel);requests+=xrtCancelRequested(cancel)?1u:0u;
            if(feeder&&i==0)assert(!xrtCancelRequested(cancel));
            xrtCancelDestroy(cancel);
        }
        assert(requests==1);assert(xrtOwnershipScopeEnd(&freeze));
        prepare_rooted(plan,&context);++resurrections;
        assert(xrtPromiseResolve(cleanupPromise,&payload));prepare_until(plan,&context,true);
    }
    quiet_freeze(executor,&freeze);
    for(unsigned i=0;i<size;++i)assert(xrtFutureState(outputs[i])==XFUTURE_CANCELLED&&
        xrtAtomic64Load(&caught[i],XMEMORY_ACQUIRE)==1&&xrtAtomic64Load(&finalized[i],XMEMORY_ACQUIRE)==1&&!xrtAtomic64Load(&after[i],XMEMORY_ACQUIRE));
    assert(xrtOwnershipScopeEnd(&freeze));deadline=xrtDeadlineAfter(5000000);
    for(;;){
        context.busy=false;XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);
        if(step==XL6_OWNERSHIP_STEP_ERROR){assert(context.busy);xrtClearError();}
        else {assert(step==XL6_OWNERSHIP_STEP_BUSY||step==XL6_OWNERSHIP_STEP_READY);context.busy=false;
            XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCommit(plan);if(state==XL6_OWNERSHIP_PLAN_READY)break;
            if(state==XL6_OWNERSHIP_PLAN_ERROR){assert(context.busy);xrtClearError();}
            else assert(state==XL6_OWNERSHIP_PLAN_BUSY||state==XL6_OWNERSHIP_PLAN_PREPARATIONS);
        }
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    assert(xl6OwnershipPlanFinish(plan)&&xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);owner=NULL;
    xrtPromiseDestroy(cleanupPromise);xrtFutureDestroy(cleanup);assert(!xrtGetError());balanced(&before);++completed;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned round=0;round<5;++round){
        for(unsigned size=1;size<=3;++size)for(unsigned cleanup=0;cleanup<2;++cleanup)for(unsigned root=0;root<2;++root)cycle(size,cleanup!=0,root,false);
        for(unsigned cleanup=0;cleanup<2;++cleanup)for(unsigned root=0;root<2;++root)cycle(3,cleanup!=0,root,true);
    }
    assert(completed==80&&refusals==320&&resurrections==40);
    printf("Generated executor ownership: 60 real worker-driven rings, 20 feeders, 320 executor/queue/frame/Watch refusals, 40 cleanup-await resurrections; native roots respected, normal catch/finally, joined workers and exact memory balance\n");
    return 0;
}
