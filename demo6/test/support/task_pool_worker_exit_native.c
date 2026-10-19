/* Included after one actual pinned implementation, never a native stub.
 * Observer pointers stored in ThreadKey borrow this caller-owned case through
 * join; they do not claim a language RC edge or a captured module owner. */
#include <assert.h>
#include <stdio.h>
typedef struct ExitTest {
    size_t refs;
    xtaskpool* pool;
    xthreadkey* key;
    uint64 worker;
    xatomic32 exits,drops,gate,jobs,late,lateGate,lateDone;
} ExitTest;
static uint32 load(const xatomic32* state){return xrtAtomic32Load(state,XMEMORY_ACQUIRE);}
static void freeze(xrtownershipscope* scope)
{
    xdeadline until=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xrtDeadlineExpired(until));xrtThreadYield();}
}
static void signal_wait(const xatomic32* state)
{
    xdeadline until=xrtDeadlineAfter(5000000);
    while(!load(state)){assert(!xrtDeadlineExpired(until));xrtThreadYield();}
}
static bool count(const void* raw,size_t* output){*output=((const ExitTest*)raw)->refs;return *output!=0;}
static bool trace(const void* raw,xrtownershipvisitor visit,ptr context){(void)raw;(void)context;return visit!=NULL;}
static const xrtownershipops ops={count,trace};
static void exit_worker(ptr raw)
{
    ExitTest* test=raw;assert(xrtThreadCurrentId()==test->worker);
    assert(xrtTaskPoolTryJoinV1(test->pool)==XWAIT_ERROR);xrtClearError();
    xrtAtomic32Store(&test->exits,1,XMEMORY_RELEASE);signal_wait(&test->gate);
}
static void drop(ptr raw,ptr context)
{
    ExitTest* test=raw;assert(!context&&test->refs==2);xrtownershipscope mutation={0};
    assert(xrtOwnershipMutationBegin(&mutation));--test->refs;assert(xrtOwnershipScopeEnd(&mutation));
    xrtAtomic32FetchAdd(&test->drops,1,XMEMORY_RELEASE);
}
static const xtaskpoolworkerexitownershipv1 policy={sizeof(policy),exit_worker,drop,&ops};
static const xtaskpoolworkerexitownershipv1 copy={sizeof(copy),exit_worker,drop,&ops};
static const xrtownershipadapterv1* adapter(xrtownershipref ref,const xrtownershippreparationv1** preparation)
{const xtaskpoolworkerexitownershipv1* known[]={&policy};return xrtTaskPoolOwnershipAdapterV2(ref,known,1,preparation);}
static void busy(ExitTest* test)
{
    xrtownershipscope frozen={0};freeze(&frozen);xrtownershipref ref=xrtTaskPoolOwnership(test->pool);size_t refs;
    assert(!ref.Ops->Count(ref.Data,&refs)&&xrtTaskPoolOwnershipQuiescenceV1Get()->Busy(ref.Data));
    const xrtownershippreparationv1* sentinel=(const void*)test;
    assert(!adapter(ref,&sentinel)&&sentinel==(const void*)test);assert(xrtOwnershipScopeEnd(&frozen));
}
static void late(ptr raw)
{
    ExitTest* test=raw;assert(xrtThreadCurrentId()==test->worker&&load(&test->exits));
    xrtAtomic32Store(&test->late,1,XMEMORY_RELEASE);signal_wait(&test->lateGate);
    xrtAtomic32Store(&test->lateDone,1,XMEMORY_RELEASE);
}
static xtaskoutcome job(xcancel* cancel,ptr raw,xtaskvalue* result)
{
    (void)cancel;(void)result;ExitTest* test=raw;test->worker=xrtThreadCurrentId();
    ptr old=xrtThreadKeyGet(test->key);assert(!old||old==test);if(!old)assert(xrtThreadKeySet(test->key,test));
    xrtAtomic32FetchAdd(&test->jobs,1,XMEMORY_RELEASE);return XTASK_SUCCESS;
}
static void join(xtaskpool* pool)
{
    xdeadline until=xrtDeadlineAfter(5000000);
    for(;;){xwaitresult result=xrtTaskPoolTryJoinV1(pool);if(result==XWAIT_OK)return;
        assert(result==XWAIT_TIMEOUT&&!xrtDeadlineExpired(until));xrtThreadYield();}
}
typedef struct Edge {const ExitTest* test;unsigned count;} Edge;
static bool visit(xrtownershipref ref,ptr raw)
{Edge* edge=raw;assert(ref.Data==edge->test&&ref.Ops==&ops);++edge->count;return true;}
static void roundtrip(void)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);ExitTest test={.refs=2};
    test.key=xrtThreadKeyCreate(late);assert(test.key);xtaskpoolconfig config={1,4,0};
    test.pool=xrtTaskPoolCreateOwnedExitV1(&config,&test,&policy);assert(test.pool&&__xrtTaskPoolAcquireResource(test.pool));
    for(unsigned i=0;i<2;++i){xfuture* future=xrtTaskSubmit(test.pool,job,&test,NULL);
        assert(future&&xrtFutureWaitFor(future,5000000)==XWAIT_OK);xrtFutureDestroy(future);assert(!load(&test.exits)&&!load(&test.late));}
    assert(xrtTaskPoolClose(test.pool)&&xrtTaskPoolWaitFor(test.pool,5000000)==XWAIT_OK);
    assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_TIMEOUT&&!load(&test.exits));
    xrtownershipref ref=xrtTaskPoolOwnership(test.pool);const xrtownershipadapterv1* admitted=NULL;
    const xrtownershippreparationv1* preparation=NULL;xrtownershipscope frozen={0};
    xdeadline until=xrtDeadlineAfter(5000000);
    for(;;){freeze(&frozen);size_t refs;if(ref.Ops->Count(ref.Data,&refs)){assert(refs==2);break;}
        assert(xrtOwnershipScopeEnd(&frozen)&&!xrtDeadlineExpired(until));xrtThreadYield();}
    preparation=(const void*)&test;assert(!xrtTaskPoolOwnershipAdapterV1(ref,&preparation)&&preparation==(const void*)&test);
    const xtaskpoolworkerexitownershipv1* wrong[]={&copy};
    assert(!xrtTaskPoolOwnershipAdapterV2(ref,wrong,1,&preparation)&&preparation==(const void*)&test);
    admitted=adapter(ref,&preparation);assert(admitted&&preparation);Edge edge={&test,0};
    assert(ref.Ops->Trace(ref.Data,visit,&edge)&&edge.count==1&&admitted->Hold(ref.Data));
    assert(xrtOwnershipScopeEnd(&frozen));__xrtTaskPoolReleaseResource(test.pool);
    until=xrtDeadlineAfter(5000000);
    while(!load(&test.exits)){assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_TIMEOUT&&!xrtDeadlineExpired(until));xrtThreadYield();}
    busy(&test);assert(!__xrtTaskPoolAcquireResource(test.pool));xrtClearError();xrtAtomic32Store(&test.gate,1,XMEMORY_RELEASE);
    until=xrtDeadlineAfter(5000000);
    while(!load(&test.late)){assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_TIMEOUT&&!xrtDeadlineExpired(until));xrtThreadYield();}
    assert(xrtTaskPoolTryJoinV1(test.pool)==XWAIT_TIMEOUT);busy(&test);assert(!load(&test.drops)&&!load(&test.lateDone));
    xrtAtomic32Store(&test.lateGate,1,XMEMORY_RELEASE);assert(xrtMemDebugFailAfter(0));join(test.pool);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();assert(load(&test.lateDone));
    xtaskpoolstats stats={0};assert(xrtTaskPoolGet(test.pool,&stats)&&stats.Submitted==2&&stats.Completed==2&&stats.Succeeded==2);
    assert(xrtTaskPoolWaitFor(test.pool,0)==XWAIT_OK&&xrtTaskPoolDestroy(test.pool));assert(test.refs==1&&load(&test.drops)==1);
    freeze(&frozen);size_t refs;assert(ref.Ops->Count(ref.Data,&refs)&&refs==1);assert(xrtOwnershipScopeEnd(&frozen));
    admitted->Drop(ref.Data);assert(xrtThreadKeyDestroy(test.key));xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    for(unsigned i=0;i<16;++i)roundtrip();
    puts("Pinned pool exit: 16 actual-worker/resource/ThreadKey-tail rounds, exact policy/pin/bytes balance; no managed language completion claim");
    return 0;
}
