/* The actual queue/storage contract: publication callbacks run outside its
 * lock/mutation, but execution cannot steal the owned frame until they finish. */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_MODULE_FUTURE
#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_queue.h"
#include <assert.h>
#include <stdio.h>
typedef struct Job {
    XL6_ContinuationTicket ticket;
    XL6_ContinuationQueue* queue;
    xatomic32 refs, entered, proceed, published, runs, retired;
} Job;
static uint32 atomicCount(const xatomic32* p) {return xrtAtomic32Load(p,XMEMORY_ACQUIRE);}
static bool retain(const void* data)
{Job* j=(Job*)data;assert(xrtAtomic32FetchAdd(&j->refs,1,XMEMORY_ACQ_REL)>0);return true;}
static void drop(const void* data)
{Job* j=(Job*)data;assert(xrtAtomic32FetchAdd(&j->refs,UINT32_MAX,XMEMORY_ACQ_REL)>0);}
static xrtownershipref ownership(const void* data) {(void)data;return (xrtownershipref){0};}
static void published(const void* data)
{
    Job* j=(Job*)data;xrtownershipscope freeze={0};
    assert(xrtOwnershipFreezeTryBegin(&freeze)); /* not called inside Commit's scope */
    size_t count=0;xrtownershipref ref=xl6ContinuationQueueOwnership(j->queue);
    assert(!ref.Ops->Count(ref.Data,&count)); /* transient PUBLISHING is never admitted */
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(!xl6ContinuationRunOne(j->queue)); /* takes its lock: no inline execution */
    xrtAtomic32Store(&j->entered,1,XMEMORY_RELEASE);
    while(!atomicCount(&j->proceed))xrtThreadYield();
    assert(!atomicCount(&j->runs)&&!atomicCount(&j->retired)&&atomicCount(&j->refs)==2);
    xrtAtomic32Store(&j->published,1,XMEMORY_RELEASE);
}
static const XL6_ContinuationStorageV2 storage={retain,drop,ownership,NULL,NULL,NULL,NULL,published};
static void run(void* data)
{
    Job* j=data;assert(atomicCount(&j->published));
    assert(xrtAtomic32FetchAdd(&j->runs,1,XMEMORY_ACQ_REL)==0);
    assert(xl6ContinuationComplete(&j->ticket));
}
static void retire(void* data)
{Job* j=data;assert(atomicCount(&j->runs)==1);assert(xrtAtomic32FetchAdd(&j->retired,1,XMEMORY_ACQ_REL)==0);}
static int32 commit(void* data)
{Job* j=data;xl6ContinuationCommit(&j->ticket);assert(!xrtGetError());return 0;}
static void exercise(bool earlyReady,bool earlyRelease,bool close)
{
    Job j={0};j.queue=xl6ContinuationQueueCreate(1);assert(j.queue);
    xrtAtomic32Store(&j.refs,2,XMEMORY_RELEASE); /* real host + transferred execution references */
    assert(xl6ContinuationReserve(j.queue,&j.ticket,run,retire,&j)==XL6_CONTINUATION_ADMITTED);
    j.ticket.storage=&storage; /* private producer binding, before any event */
    if(earlyReady)xl6ContinuationReady(&j.ticket);
    if(earlyRelease)xl6ContinuationTransportReleased(&j.ticket);
    xthread* thread=xrtThreadCreate(commit,&j,0);assert(thread);
    while(!atomicCount(&j.entered))xrtThreadYield();
    if(!earlyReady)xl6ContinuationReady(&j.ticket);
    if(!earlyRelease)xl6ContinuationTransportReleased(&j.ticket);
    if(close)xl6ContinuationQueueClose(j.queue);
    assert(!xl6ContinuationRunOne(j.queue)&&atomicCount(&j.refs)==2&&!atomicCount(&j.runs));
    xrtAtomic32Store(&j.proceed,1,XMEMORY_RELEASE);
    assert(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK);
    assert(xrtThreadExitCode(thread)==0);xrtThreadDestroy(thread);
    assert(xl6ContinuationRunOne(j.queue)&&atomicCount(&j.runs)==1&&atomicCount(&j.retired)==1&&atomicCount(&j.refs)==1);
    assert(!xl6ContinuationRunOne(j.queue));
    xl6ContinuationQueueClose(j.queue);assert(xl6ContinuationQueueDestroy(j.queue));
    drop(&j);assert(!atomicCount(&j.refs)&&!xrtGetError());
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    for(unsigned round=0;round<50;++round)for(unsigned mode=0;mode<8;++mode)
        exercise((mode&1)!=0,(mode&2)!=0,(mode&4)!=0);
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("Continuation publication: 400 two-thread commit/source/close handshakes, callback outside queue lock/mutation, transient ownership refused, no premature execution, exactly one retirement, balanced");
    return 0;
}
