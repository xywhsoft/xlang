/* Private child process only: INT is raised only while a watch is installed.
 * Including the actual adapter allows deterministic overflow/race witnesses;
 * no production hook, substitute queue, or alternative algorithm is used. */
#include "src/runtime/xl6_signal_queue.c"
#include <assert.h>
#include <stdio.h>

static void error(xerrkind kind)
{ assert(xrtErrorKind(xrtGetError())==kind);xrtClearError(); }
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void inject(SignalQueue* q, uint32 count, uint64 total)
{
    xsignalevent event={XSIGNAL_INT,71,count,total,123456,"INT"};
    record(q->watch,&event,q);
}
static void fields(void* e, uint64 count, uint64 total, bool coalesced, bool saturated)
{
    assert(xl6SignalEventCodeV1(e)==XSIGNAL_INT && xl6SignalEventSystemCodeV1(e)==71);
    assert(xl6SignalEventCountV1(e)==count && xl6SignalEventTotalV1(e)==total);
    assert(xl6SignalEventTimeV1(e)==123456);
    assert(xl6SignalEventCoalescedV1(e)==coalesced && xl6SignalEventSaturatedV1(e)==saturated);
    xstrview name=xl6SignalEventNameV1(e);
    assert(name.Data && name.Size==3 && !memcmp(name.Data,"INT",3));xrtFree((void*)name.Data);
}
static void queueTransactions(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    SignalQueue* q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,2);assert(q);
    assert(xl6SignalQueueActiveV1(q) && !xl6SignalQueueClosedV1(q));
    assert(xl6SignalQueueCodeV1(q)==XSIGNAL_INT && !xl6SignalQueuePendingV1(q));
    assert(!xl6SignalQueuePrepareV1(q,0));
    inject(q,1,1);inject(q,2,3);inject(q,3,6);
    assert(xl6SignalQueuePendingV1(q)==2 && xl6SignalQueuePrepareV1(q,0));
    assert(xl6SignalQueuePrepareV1(q,0));
    void* first=xl6SignalQueueSnapshotV1(q);assert(first);fields(first,1,1,false,false);
    inject(q,4,10);inject(q,5,15);
    void* same=xl6SignalQueueSnapshotV1(q);assert(same);fields(same,1,1,false,false);
    assert(xl6SignalQueuePendingV1(q)==3); /* capacity plus immutable prepared slot */
    xl6SignalEventDestroyV1(same);xl6SignalQueueConsumeV1(q);
    assert(xl6SignalQueuePrepareV1(q,0));
    void* second=xl6SignalQueueSnapshotV1(q);assert(second);fields(second,5,6,true,false);
    xl6SignalEventDestroyV1(second);xl6SignalQueueConsumeV1(q);
    assert(xl6SignalQueuePrepareV1(q,0));
    assert(xrtMemDebugFailAfter(0));assert(!xl6SignalQueueSnapshotV1(q));
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();error(XERR_MEMORY);
    assert(xl6SignalQueuePendingV1(q)==1 && xl6SignalQueuePrepareV1(q,0));
    void* third=xl6SignalQueueSnapshotV1(q);assert(third);fields(third,9,15,true,false);
    xl6SignalEventDestroyV1(third);xl6SignalQueueConsumeV1(q);
    assert(!xl6SignalQueuePrepareV1(q,0));
    assert(!xl6SignalQueueSnapshotV1(q));error(XERR_STATE);
    xl6SignalQueueConsumeV1(q);error(XERR_STATE);
    assert(!xl6SignalQueuePrepareV1(q,-2));error(XERR_RANGE);
    assert(!xl6SignalQueuePrepareV1(q,INT64_MAX));error(XERR_RANGE);
    assert(xl6SignalQueueCloseV1(q) && xl6SignalQueueCloseV1(q));
    assert(xl6SignalQueueClosedV1(q) && !xl6SignalQueueActiveV1(q));
    assert(!xl6SignalQueuePrepareV1(q,-1));
    assert(!xl6SignalQueueSnapshotV1(q));error(XERR_STATE);
    xl6SignalQueueDestroyV1(q);fields(first,1,1,false,false);xl6SignalEventDestroyV1(first);
    xrtSignalShutdown();assert(!xrtGetError());balanced(before);
    puts("signal queue: bounded coalescing, immutable snapshots and OOM retry, balanced");
}
static void saturation(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    SignalQueue* q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,1);assert(q);
    inject(q,1,UINT64_MAX-1);q->records[0].count=UINT64_MAX-1;
    assert(xrtMemDebugFailAfter(0));inject(q,2,UINT64_MAX);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear(); /* recorder never allocates */
    assert(xl6SignalQueuePrepareV1(q,0));
    void* event=xl6SignalQueueSnapshotV1(q);assert(event);fields(event,UINT64_MAX,UINT64_MAX,true,true);
    xl6SignalQueueConsumeV1(q);xl6SignalQueueDestroyV1(q);xl6SignalEventDestroyV1(event);
    xrtSignalShutdown();assert(!xrtGetError());balanced(before);
    puts("signal queue: uint64 saturation and allocation-free native recorder, balanced");
}
static int32 wrongThread(void* data)
{
    assert(!xl6SignalQueueActiveV1(data));error(XERR_STATE);
    assert(!xl6SignalQueueClosedV1(data));error(XERR_STATE);
    assert(!xl6SignalQueueCodeV1(data));error(XERR_STATE);
    assert(!xl6SignalQueuePendingV1(data));error(XERR_STATE);
    assert(!xl6SignalQueuePrepareV1(data,0));error(XERR_STATE);
    assert(!xl6SignalQueueSnapshotV1(data));error(XERR_STATE);
    xl6SignalQueueConsumeV1(data);error(XERR_STATE);
    assert(!xl6SignalQueueCloseV1(data));error(XERR_STATE);
    xl6SignalQueueCheckOwnerV1(data);error(XERR_STATE);
    return 0;
}
static int32 foreignDestroy(void* data)
{ xl6SignalQueueDestroyV1(data);assert(!xrtGetError());return 0; }
static void ownership(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    SignalQueue* q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,2);assert(q);
    xthread* thread=xrtThreadCreate(wrongThread,q,0);assert(thread);
    assert(xrtThreadWait(thread)==XWAIT_OK && !xrtThreadExitCode(thread));xrtThreadDestroy(thread);
    assert(xl6SignalQueueActiveV1(q));
    xrtSetErrorInfo(XERR_STATE,"signal-queue-test",1,"ambient");
    const xerror* prior=xrtGetError();xl6SignalQueueDestroyV1(q);
    assert(xrtGetError()==prior);xrtClearError();
    q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,2);assert(q);
    thread=xrtThreadCreate(foreignDestroy,q,0);assert(thread);
    assert(xrtThreadWait(thread)==XWAIT_OK && !xrtThreadExitCode(thread));xrtThreadDestroy(thread);
    xrtSignalShutdown();assert(!xrtGetError());balanced(before);
    puts("signal queue: nine owner-thread guards, ambient error and foreign destruction, balanced");
}
static void constructors(void)
{
    unsigned hits=0, completions=0;
    for(unsigned kind=0;kind<6;++kind) {
        bool complete=false;
        for(unsigned point=0;point<256;++point) {
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point));
            void* q=xl6SignalQueueCreateV1(kind==0?XSIGNAL_INT:kind==1?0:kind==2?INT64_MAX:XSIGNAL_INT,
                false,kind==3?0:kind==4?65537:kind==5?INT64_MAX:2);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            if(hit) {assert(!q && xrtErrorKind(xrtGetError())==XERR_MEMORY);++hits;}
            else {assert(kind==0 ? q!=NULL : q==NULL);++completions;complete=true;}
            xl6SignalQueueDestroyV1(q);xrtClearError();xrtSignalShutdown();
            assert(!xrtGetError());balanced(before);
            if(!hit)break;
        }
        assert(complete);
    }
    printf("signal queue constructors: %u actual allocation failures, %u first no-hit completions, balanced\n",hits,completions);
}
static void nativeDelivery(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<32;++i) {
        SignalQueue* q=xl6SignalQueueCreateV1(XSIGNAL_INT,true,2);assert(q);
        assert(xrtSignalRaise(XSIGNAL_INT));
        assert(xl6SignalQueuePrepareV1(q,2000));
        void* event=xl6SignalQueueSnapshotV1(q);assert(event);
        assert(xl6SignalEventCountV1(event)>0 && xl6SignalEventCodeV1(event)==XSIGNAL_INT);
        xl6SignalQueueConsumeV1(q);assert(!xl6SignalQueuePrepareV1(q,-1));
        assert(!xl6SignalQueueActiveV1(q));xl6SignalQueueDestroyV1(q);
        assert(xl6SignalEventCountV1(event)>0);xl6SignalEventDestroyV1(event);
        xrtSignalShutdown();assert(!xrtGetError());
    }
    balanced(before);puts("signal queue: 32 real once deliveries and snapshot lifetime, balanced");
}
typedef struct Delayed {SignalQueue* q;xatomic32 entered,release;} Delayed;
static void delayed(xsignalwatch* watch,const xsignalevent* event,void* data)
{
    Delayed* state=data;xrtAtomic32Store(&state->entered,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&state->release,XMEMORY_ACQUIRE))xrtThreadYield();
    record(watch,event,state->q);
}
static int32 releaseRecorder(void* data)
{
    Delayed* state=data;
    while(!xrtAtomic32Load(&state->entered,XMEMORY_ACQUIRE))xrtThreadYield();
    xrtSleep(20);xrtAtomic32Store(&state->release,1,XMEMORY_RELEASE);return 0;
}
static void onceRace(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    SignalQueue* q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,2);assert(q);
    xrtSignalFree(q->watch);
    Delayed state={q,XRT_ATOMIC32_INIT(0),XRT_ATOMIC32_INIT(0)};
    q->watch=xrtSignalOnce(XSIGNAL_INT,delayed,&state);assert(q->watch);
    xthread* thread=xrtThreadCreate(releaseRecorder,&state,0);assert(thread);
    assert(xrtSignalRaise(XSIGNAL_INT));
    while(!xrtAtomic32Load(&state.entered,XMEMORY_ACQUIRE))xrtThreadYield();
    assert(!xrtSignalActive(q->watch)); /* inactive BEFORE native recorder */
    assert(xl6SignalQueuePrepareV1(q,0)); /* synchronizes in-flight recorder */
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    xl6SignalQueueConsumeV1(q);xl6SignalQueueDestroyV1(q);xrtSignalShutdown();
    assert(!xrtGetError());balanced(before);
    puts("signal queue: inactive-once/in-flight-recorder race synchronized, balanced");
}
static void closeRace(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    SignalQueue* q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,2);assert(q);
    xrtSignalFree(q->watch);
    Delayed state={q,XRT_ATOMIC32_INIT(0),XRT_ATOMIC32_INIT(0)};
    q->watch=xrtSignalOn(XSIGNAL_INT,delayed,&state);assert(q->watch);
    xthread* thread=xrtThreadCreate(releaseRecorder,&state,0);assert(thread);
    assert(xrtSignalRaise(XSIGNAL_INT));
    while(!xrtAtomic32Load(&state.entered,XMEMORY_ACQUIRE))xrtThreadYield();
    assert(xl6SignalQueueCloseV1(q)); /* waits without holding the queue mutex */
    assert(xrtAtomic32Load(&state.release,XMEMORY_ACQUIRE));
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(!xl6SignalQueuePendingV1(q));xl6SignalQueueDestroyV1(q);xrtSignalShutdown();
    assert(!xrtGetError());balanced(before);
    puts("signal queue: close synchronously retires active recorder without deadlock, balanced");
}
typedef struct Producer {SignalQueue* q;xatomic32 done;} Producer;
static int32 produce(void* data)
{
    Producer* state=data;
    for(unsigned i=1;i<=10000;++i)inject(state->q,1,i);
    xrtAtomic32Store(&state->done,1,XMEMORY_RELEASE);return 0;
}
static void concurrentProducer(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    SignalQueue* q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,4);assert(q);
    Producer state={q,XRT_ATOMIC32_INIT(0)};
    xthread* thread=xrtThreadCreate(produce,&state,0);assert(thread);
    uint64 count=0;
    while(!xrtAtomic32Load(&state.done,XMEMORY_ACQUIRE) || xl6SignalQueuePendingV1(q)) {
        if(xl6SignalQueuePrepareV1(q,1)) {
            void* event=xl6SignalQueueSnapshotV1(q);assert(event);
            count+=xl6SignalEventCountV1(event);xl6SignalEventDestroyV1(event);
            xl6SignalQueueConsumeV1(q);
        }
    }
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);assert(count==10000);
    xl6SignalQueueDestroyV1(q);xrtSignalShutdown();assert(!xrtGetError());balanced(before);
    puts("signal queue: 10000 concurrent recorder events conserve count, balanced");
}
static int32 spuriousWake(void* data)
{
    SignalQueue* q=data;
    for(unsigned i=0;i<40;++i) {lock(q);assert(xrtCondSignal(q->changed));unlock(q);xrtSleep(1);}
    return 0;
}
static void absoluteDeadline(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    SignalQueue* q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,2);assert(q);
    xthread* thread=xrtThreadCreate(spuriousWake,q,0);assert(thread);
    uint64 start=xrtClock();assert(!xl6SignalQueuePrepareV1(q,30));uint64 elapsed=xrtClock()-start;
    assert(elapsed>=UINT64_C(25000) && elapsed<UINT64_C(1000000));
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    xl6SignalQueueDestroyV1(q);xrtSignalShutdown();assert(!xrtGetError());balanced(before);
    puts("signal queue: spurious wakes do not extend the absolute timeout, balanced");
}
static int32 retireWhileWaiting(void* data)
{
    unsigned action=*(unsigned*)data;xrtSleep(20);
    if(action==0)assert(xrtSignalRestore(XSIGNAL_INT));
    else if(action==1)assert(xrtSignalIgnore(XSIGNAL_INT));
    else xrtSignalShutdown();
    assert(!xrtGetError());return 0;
}
static void globalRetirement(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(unsigned action=0;action<3;++action) {
        SignalQueue* q=xl6SignalQueueCreateV1(XSIGNAL_INT,false,2);assert(q);
        xthread* thread=xrtThreadCreate(retireWhileWaiting,&action,0);assert(thread);
        uint64 start=xrtClock();assert(!xl6SignalQueuePrepareV1(q,-1));
        assert(xrtClock()-start<UINT64_C(2000000));
        assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
        xl6SignalQueueDestroyV1(q);xrtSignalShutdown();assert(!xrtGetError());
    }
    balanced(before);puts("signal queue: restore/ignore/shutdown end indefinite wait, balanced");
}
int main(void)
{
    assert(xrtMemDebugEnable(true));assert(xrtSignalSupported(XSIGNAL_INT));
    queueTransactions();saturation();ownership();constructors();nativeDelivery();onceRace();closeRace();
    concurrentProducer();absoluteDeadline();globalRetirement();
    puts("signal queue native contract passed");return 0;
}
