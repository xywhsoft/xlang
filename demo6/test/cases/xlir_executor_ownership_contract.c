#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>

typedef struct Resolve {xrtownershipref executor;bool omitQueue,busy;} Resolve;
static unsigned graphs,externalQueues,refusals,ooms,retainedShells,activeRefusals,entryRefusals;
static bool resolve(xrtownershipref ref,void* data,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation)
{
    Resolve* context=data;*adapter=NULL;*preparation=NULL;
    if(ref.Ops==context->executor.Ops){
        *adapter=xl6ContinuationExecutorOwnershipAdapterV1(ref,preparation);
        if(!*adapter)context->busy=true;
        return *adapter!=NULL;
    }
    if(!context->omitQueue)*adapter=xl6ContinuationQueueOwnershipAdapterV1(ref,NULL,0,preparation);
    return *adapter!=NULL;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
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
typedef struct Edges {xrtownershipref queue;size_t count;} Edges;
static bool edge(xrtownershipref ref,void* data)
{
    Edges* edges=data;
    if(edges->count)assert(ref.Data==edges->queue.Data&&ref.Ops==edges->queue.Ops);
    edges->queue=ref;++edges->count;return true;
}
static void reclaim(XL6_OwnershipPlan* plan,Resolve* context)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){
        context->busy=false;XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);
        if(step==XL6_OWNERSHIP_STEP_ERROR){assert(context->busy);xrtClearError();}
        else {assert(step==XL6_OWNERSHIP_STEP_BUSY||step==XL6_OWNERSHIP_STEP_READY);
            context->busy=false;XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCommit(plan);
            if(state==XL6_OWNERSHIP_PLAN_READY)break;
            if(state==XL6_OWNERSHIP_PLAN_ERROR){assert(context->busy);xrtClearError();}
            else assert(state==XL6_OWNERSHIP_PLAN_BUSY||state==XL6_OWNERSHIP_PLAN_PREPARATIONS);
        }
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    assert(xl6OwnershipPlanFinish(plan)&&xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
}
static void parked_graph(unsigned workers,bool queueRoot,bool inject)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateGuaranteed(4,workers);assert(executor);
    xrtownershipref root=xl6ContinuationExecutorOwnership(executor);Resolve context={root,false,false};
    xrtownershipscope freeze={0};quiet_freeze(root,&freeze);
    Edges edges={0};size_t count;assert(root.Ops->Trace(root.Data,edge,&edges)&&edges.count==workers+1);
    assert(edges.queue.Ops->Count(edges.queue.Data,&count)&&count==workers+1);
    const xrtownershippreparationv1 *ep=NULL,*qp=NULL;
    const xrtownershipadapterv1* ea=xl6ContinuationExecutorOwnershipAdapterV1(root,&ep);
    const xrtownershipadapterv1* qa=xl6ContinuationQueueOwnershipAdapterV1(edges.queue,NULL,0,&qp);
    assert(ea&&qa&&ep&&qp);
    assert(xrtOwnershipScopeEnd(&freeze));
    context.omitQueue=true;XL6_OwnershipPlan* failed=NULL;
    assert(create_plan(root,&context,&failed)==XL6_OWNERSHIP_PLAN_ERROR&&!failed);
    xrtClearError();context.omitQueue=false;++refusals;
    if(inject)for(unsigned budget=0;budget<80;++budget){
        xmemdebugsnapshot checkpoint;xrtMemDebugSnapshot(&checkpoint);XL6_OwnershipPlan* probe=NULL;
        assert(xrtMemDebugFailAfter(budget));XL6_OwnershipPlanResult result=create_plan(root,&context,&probe);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(probe){assert(result==XL6_OWNERSHIP_PLAN_ROOTED);assert(xl6OwnershipPlanTryDestroy(probe)==XL6_OWNERSHIP_PLAN_READY);}
        else {assert(result==XL6_OWNERSHIP_PLAN_ERROR&&triggered);++ooms;}
        balanced(&checkpoint);
    }
    if(queueRoot)assert(qa->Hold(edges.queue.Data));
    XL6_OwnershipPlan* plan=NULL;
    assert(create_plan(root,&context,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY); /* native executor owner still roots it */
    ea->Drop(root.Data); /* actual owner slot transferred to the plan's real pin */
    if(queueRoot){
        assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_BUSY);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
        quiet_freeze(root,&freeze);Edges held={0};assert(root.Ops->Trace(root.Data,edge,&held)&&held.count==workers+1);
        assert(!qp->Ready(edges.queue.Data));assert(xrtOwnershipScopeEnd(&freeze));
        qa->Drop(edges.queue.Data);++externalQueues;
    }
    reclaim(plan,&context);assert(!xrtGetError());balanced(&before);++graphs;
}
static void sealed_entry_unreachable(void* data)
{(void)data;assert(!"sealed executor entry cannot execute");}
static void retained_shell(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateGuaranteed(2,2);assert(executor);
    xrtownershipref ref=xl6ContinuationExecutorOwnership(executor);xrtownershipscope freeze={0};quiet_freeze(ref,&freeze);
    const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipadapterv1* adapter=xl6ContinuationExecutorOwnershipAdapterV1(ref,&preparation);
    assert(adapter&&adapter->Hold(ref.Data));assert(xrtOwnershipScopeEnd(&freeze));
    xl6ContinuationExecutorClose(executor);assert(xl6ContinuationExecutorDestroy(executor));
    XL6_ContinuationTicket rejected={0};
    assert(xl6ContinuationExecutorIsIdle(executor));
    assert(xl6ContinuationExecutorReserve(executor,&rejected,sealed_entry_unreachable,sealed_entry_unreachable,NULL)==XL6_CONTINUATION_CLOSED);
    assert(xl6ContinuationBlockingEnter(executor)==XL6_CONTINUATION_BLOCKING_RESOURCE_FAILURE);
    xl6ContinuationExecutorClose(executor);assert(!xl6ContinuationExecutorDestroy(executor));
    quiet_freeze(ref,&freeze);size_t count;Edges edges={0};
    assert(ref.Ops->Count(ref.Data,&count)&&count==1&&ref.Ops->Trace(ref.Data,edge,&edges)&&edges.count==0&&preparation->Ready(ref.Data));
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);balanced(&before);++retainedShells;
}
typedef struct Work {XL6_ContinuationTicket ticket;xatomic64 phase,proceed;bool blockRelease;} Work;
static void pause_work(Work* work)
{
    xrtAtomic64Store(&work->phase,1,XMEMORY_RELEASE);xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtAtomic64Load(&work->proceed,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void run_work(void* data) {Work* work=data;if(!work->blockRelease)pause_work(work);}
static void release_work(void* data) {Work* work=data;if(work->blockRelease)pause_work(work);}
static void active_graph(bool blockRelease)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Work work={0};work.blockRelease=blockRelease;
    XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateGuaranteed(1,1);assert(executor);
    assert(xl6ContinuationExecutorReserve(executor,&work.ticket,run_work,release_work,&work)==XL6_CONTINUATION_ADMITTED);
    xl6ContinuationReady(&work.ticket);xl6ContinuationTransportReleased(&work.ticket);xl6ContinuationCommit(&work.ticket);
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtAtomic64Load(&work.phase,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    xrtownershipscope freeze={0};while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    xrtownershipref ref=xl6ContinuationExecutorOwnership(executor);size_t count;const xrtownershippreparationv1* preparation=NULL;
    assert(!ref.Ops->Count(ref.Data,&count)&&!xl6ContinuationExecutorOwnershipAdapterV1(ref,&preparation));++activeRefusals;
    assert(xrtOwnershipScopeEnd(&freeze));xrtAtomic64Store(&work.proceed,1,XMEMORY_RELEASE);
    xl6ContinuationExecutorClose(executor);
    while(!xl6ContinuationExecutorDestroy(executor)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    balanced(&before);
}
typedef struct EntryProbe {XL6_ContinuationExecutor* executor;unsigned acquired,released,refusals;} EntryProbe;
static int entry_acquire(void* data)
{
    EntryProbe* probe=data;++probe->acquired;xl6ContinuationExecutorClose(probe->executor);
    assert(!xl6ContinuationExecutorDestroy(probe->executor));++probe->refusals;return 1;
}
static void entry_release(void* data)
{
    EntryProbe* probe=data;++probe->released;
    assert(!xl6ContinuationExecutorDestroy(probe->executor));++probe->refusals;
}
static void entry_unreachable(void* data) {(void)data;assert(!"closed admission cannot execute");}
static void entry_graph(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);EntryProbe probe={0};
    probe.executor=xl6ContinuationExecutorCreateGuaranteed(1,1);assert(probe.executor);
    xrtownershipscope freeze={0};quiet_freeze(xl6ContinuationExecutorOwnership(probe.executor),&freeze);
    assert(xrtOwnershipScopeEnd(&freeze));XL6_CodeOwner owner={sizeof(owner),&probe,entry_acquire,entry_release};XL6_ContinuationTicket ticket={0};
    assert(xl6ContinuationExecutorReserveOwned(probe.executor,&ticket,entry_unreachable,entry_unreachable,&probe,&owner)==XL6_CONTINUATION_CLOSED);
    assert(probe.acquired==1&&probe.released==1&&probe.refusals==2);entryRefusals+=probe.refusals;
    assert(xl6ContinuationExecutorDestroy(probe.executor));assert(!xrtGetError());balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned round=0;round<20;++round){
        for(unsigned workers=1;workers<=3;++workers)for(unsigned root=0;root<2;++root)parked_graph(workers,root!=0,!root);
        retained_shell();active_graph(false);active_graph(true);entry_graph();
    }
    assert(graphs==120&&externalQueues==60&&refusals==120&&ooms>20&&retainedShells==20&&activeRefusals==40&&entryRefusals==40);
    printf("Executor ownership: 120 real parked graphs, 60 external queue roots, 120 policy refusals, 20 retained joined shells, 40 run/release refusals, 40 host acquire/drop shutdown refusals, %u real snapshot OOM failures; exact wait-slot references, cooperative close/join, zero detached workers and balanced memory\n",ooms);
    return 0;
}
