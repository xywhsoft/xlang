#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_executor.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>

/* Only the executor translation unit redirects ThreadCreate to this wrapper.
 * Count real successful OS-thread creation, not a guessed queue statistic. */
static atomic_uint threads;
xthread* fixed_thread_create(xthreadproc proc,ptr data,size_t stack)
{
    xthread* result=xrtThreadCreate(proc,data,stack);
    if(result)atomic_fetch_add(&threads,1);
    return result;
}
typedef struct Cohort {atomic_uint runs,releases;uint64_t producer;} Cohort;
typedef struct Frame {
    XL6_ContinuationTicket ticket;
    XL6_ContinuationExecutor* executor;
    struct Frame* next;
    Cohort* cohort;
    bool guard;
} Frame;
static void await_count(atomic_uint* count,unsigned expected)
{
    xdeadline deadline=xrtDeadlineAfter(10000000);
    while(atomic_load_explicit(count,memory_order_acquire)!=expected){
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
static void publish(Frame* frame)
{
    xl6ContinuationReady(&frame->ticket);
    xl6ContinuationTransportReleased(&frame->ticket);
    xl6ContinuationCommit(&frame->ticket);
}
static void run(void* data);
static void release(void* data)
{
    Frame* frame=data;
    atomic_fetch_add_explicit(&frame->cohort->releases,1,memory_order_release);
}
static void run(void* data)
{
    Frame* frame=data;
    assert(xrtThreadCurrentId()!=frame->cohort->producer);
    if(frame->guard){
        /* A fixed one-worker pool must refuse rather than silently grow. */
        assert(xl6ContinuationBlockingEnter(frame->executor)==XL6_CONTINUATION_BLOCKING_LIMIT);
    }
    if(frame->next){
        Frame* next=frame->next;
        assert(xl6ContinuationReserveSuccessor(&frame->ticket,&next->ticket,run,release,next)==XL6_CONTINUATION_ADMITTED);
        publish(next);
    }
    atomic_fetch_add_explicit(&frame->cohort->runs,1,memory_order_release);
}
static void finish(XL6_ContinuationExecutor* executor)
{
    xdeadline deadline=xrtDeadlineAfter(10000000);
    xl6ContinuationExecutorClose(executor);
    while(!xl6ContinuationExecutorDestroy(executor)){
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
typedef struct Edges {size_t count;} Edges;
static bool edge(xrtownershipref ref,void* data)
{assert(ref.Data&&ref.Ops);++((Edges*)data)->count;return true;}
static void parked(XL6_ContinuationExecutor* executor,size_t workers)
{
    xrtownershipref ref=xl6ContinuationExecutorOwnership(executor);
    xdeadline deadline=xrtDeadlineAfter(10000000);
    for(;;){
        xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            size_t count;bool quiet=ref.Ops->Count(ref.Data,&count);
            if(quiet){Edges edges={0};assert(count==1&&ref.Ops->Trace(ref.Data,edge,&edges));assert(edges.count==workers+1);}
            assert(xrtOwnershipScopeEnd(&freeze));if(quiet)return;
        }
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void cohort(size_t workers,bool successors)
{
    enum {COUNT=1024};
    xmemdebugsnapshot before;Cohort seen={0};
    unsigned started=atomic_load(&threads);xrtMemDebugSnapshot(&before);
    Frame* frames=xrtCalloc(COUNT*(successors?2:1),sizeof(*frames));assert(frames);
    XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(COUNT,workers);assert(executor);
    size_t actual=workers?workers:4;parked(executor,actual);
    assert(atomic_load(&threads)==started+actual);seen.producer=xrtThreadCurrentId();
    assert(xrtMemDebugFailAfter(0));
    for(size_t i=0;i<COUNT;++i){
        frames[i].executor=executor;frames[i].cohort=&seen;
        if(successors){frames[i].next=&frames[COUNT+i];frames[COUNT+i].executor=executor;frames[COUNT+i].cohort=&seen;}
        assert(xl6ContinuationExecutorReserve(executor,&frames[i].ticket,run,release,&frames[i])==XL6_CONTINUATION_ADMITTED);
        xl6ContinuationCommit(&frames[i].ticket);
    }
    Frame extra={0};
    assert(xl6ContinuationExecutorReserve(executor,&extra.ticket,run,release,&extra)==XL6_CONTINUATION_FULL);
    assert(!xrtMemDebugFailTriggered()&&atomic_load(&threads)==started+actual);
    assert(atomic_load(&seen.runs)==0&&!xl6ContinuationExecutorIsIdle(executor));
    xl6ContinuationExecutorClose(executor);
    assert(!xl6ContinuationExecutorDestroyCompleted(executor));
    assert(xl6ContinuationExecutorReserve(executor,&extra.ticket,run,release,&extra)==XL6_CONTINUATION_CLOSED);
    for(size_t i=0;i<COUNT;++i){
        /* Both last-event orders: producers signal only, never run callbacks. */
        if(i&1){xl6ContinuationReady(&frames[i].ticket);xl6ContinuationTransportReleased(&frames[i].ticket);}
        else {xl6ContinuationTransportReleased(&frames[i].ticket);xl6ContinuationReady(&frames[i].ticket);}
    }
    unsigned expected=COUNT*(successors?2:1);
    await_count(&seen.releases,expected);await_count(&seen.runs,expected);
    assert(!xrtMemDebugFailTriggered()&&atomic_load(&threads)==started+actual);
    xrtMemDebugFailClear();finish(executor);xrtFree(frames);balanced(&before);
}
int main(void)
{
    unsigned failures=0;
    assert(xrtMemDebugEnable(true));
    assert(!xl6ContinuationExecutorCreateFixedV1(0,0));
    for(unsigned round=0;round<4;++round)for(size_t workers=0;workers<3;++workers){
        cohort(workers,false);cohort(workers,true);
    }
    for(unsigned budget=0;budget<80;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(budget));
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(8,2);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(triggered){assert(!executor);++failures;}else {assert(executor);finish(executor);}
        balanced(&before);if(!triggered)break;
    }
    assert(failures>0&&failures<80);
    for(unsigned round=0;round<20;++round){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Cohort seen={0};Frame frame={0},discard={0};
        unsigned started=atomic_load(&threads);
        frame.executor=xl6ContinuationExecutorCreateFixedV1(2,0);assert(frame.executor);parked(frame.executor,2);
        assert(atomic_load(&threads)==started+2); /* zero default is bounded by capacity */
        finish(frame.executor);
        frame.executor=xl6ContinuationExecutorCreateFixedV1(2,1);assert(frame.executor);parked(frame.executor,1);
        frame.cohort=&seen;frame.guard=true;seen.producer=xrtThreadCurrentId();
        assert(xl6ContinuationExecutorReserve(frame.executor,&discard.ticket,run,release,&discard)==XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationAbort(&discard.ticket));
        assert(xl6ContinuationExecutorReserve(frame.executor,&frame.ticket,run,release,&frame)==XL6_CONTINUATION_ADMITTED);
        publish(&frame);await_count(&seen.releases,1);finish(frame.executor);balanced(&before);
    }
    printf("fixed executor: 24 cohorts of 1024 dormant roots, 12288 successors after Close, real fixed thread counts, allocation-free admission/delivery, %u creation-prefix failures, 20 abort/last-worker refusal cases; exact memory balance\n",failures);
    return 0;
}
