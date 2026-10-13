#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_SEM
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void worker_completion_prepare(void), worker_completion_finish(void);
extern xfuture* worker_completion_launch(unsigned, int);
extern void worker_completion_check(unsigned, xfuture*);
static xsem *entered, *proceed, *dropEntered, *dropProceed, *observed, *watchFinished, *resultFinished, *inputsFinished;
static xatomic32 entries, bodies, firstDrops, unusedDrops, resultDrops, publications, watchDrops, boxFailures;
static uint64 mainThread;
static unsigned kind, executions, faultExecutions, observations, lateCancels, discarded;
static int bodyMode, firstMode, unusedMode, resultMode, cancelPoint, boxFault, smoke;
static xfuture* activeOutput;
/* Added lane: arm the REAL worker's allocation budget at its exact generated
 * payload-allocation entry, and leave it armed through body/retirement/publication.
 * The completion observer clears that same worker's TLS before bookkeeping. */
static bool scanFault;
static unsigned scanBudget,scanRuns,scanHits;
static xatomic32 scanTriggered;
static void release_watch(void*);
static xfuturestate expectedState;
static xerrkind expectedKind;
static int expectedCode;
static unsigned count(xatomic32* n) { return xrtAtomic32Load(n, XMEMORY_SEQ_CST); }
static void worker_entry(void)
{
    if (xrtAtomic32FetchAdd(&entries, 1, XMEMORY_SEQ_CST) == 0) {
        assert(xrtSemPost(entered)); assert(xrtSemWaitFor(proceed, 15000000) == XWAIT_OK);
    }
}
void* worker_completion_malloc(size_t size)
{
    if (xrtThreadCurrentId() != mainThread && count(&entries) == 0) {
        void* value; worker_entry();
        if (scanFault) assert(xrtMemDebugFailAfter(scanBudget));
        else if (boxFault) {
            assert(xrtMemDebugFailAfter(0)); value = xrtMalloc(size);
            assert(value == NULL && xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
            assert(xrtAtomic32FetchAdd(&boxFailures, 1, XMEMORY_SEQ_CST) == 0); return value;
        }
    }
    return xrtMalloc(size);
}
int worker_completion_enter_body(void)
{
    if (count(&entries) == 0) worker_entry();
    assert(xrtAtomic32FetchAdd(&bodies, 1, XMEMORY_SEQ_CST) == 0); return bodyMode;
}
int worker_completion_drop_mode(int64_t id)
{
    xatomic32* counter; int mode;
    assert(id == 7 || id == 9 || id == 17);
    if (id == 9 && cancelPoint == 2) {
        assert(activeOutput && !xrtFutureDone(activeOutput));
        assert(xrtSemPost(dropEntered)); assert(xrtSemWaitFor(dropProceed, 15000000) == XWAIT_OK);
    }
    counter = id == 7 ? &firstDrops : id == 9 ? &unusedDrops : &resultDrops;
    mode = id == 7 ? firstMode : id == 9 ? unusedMode : resultMode;
    assert(xrtAtomic32FetchAdd(counter, 1, XMEMORY_SEQ_CST) == 0);
    if (id == 17) assert(xrtSemPost(resultFinished));
    else assert(xrtSemPost(inputsFinished));
    return mode;
}
static void check_outcome(void)
{
    const xerror* error = xrtFutureError(activeOutput);
    if (xrtFutureState(activeOutput) != expectedState || (expectedKind && (!error || xrtErrorKind(error) != expectedKind)) ||
        (expectedCode && (!error || xrtErrorCode(error) != expectedCode))) {
        fprintf(stderr, "worker outcome kind=%u body=%d drops=%d/%d/%d cancel=%d fault=%d: state=%d error=%d/%d expected=%d %d/%d\n",
            kind, bodyMode, firstMode, unusedMode, resultMode, cancelPoint, boxFault, (int)xrtFutureState(activeOutput),
            error ? xrtErrorKind(error) : 0, error ? xrtErrorCode(error) : 0, expectedState, expectedKind, expectedCode);
        assert(0);
    }
    if (!expectedKind) assert(!error);
}
static void observe(void* data)
{
    (void)data;
    if(scanFault){
        assert(xrtThreadCurrentId()!=mainThread);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xrtAtomic32Store(&scanTriggered,hit?1:0,XMEMORY_SEQ_CST);
        const xerror* error=xrtFutureError(activeOutput);
        if(hit){
            assert(xrtFutureState(activeOutput)==XFUTURE_FAILED);
            /* A failed diagnostic allocation may be represented by the
             * established asynchronous no-diagnostic InternalError fallback. */
            assert(!error || xrtErrorKind(error)==XERR_MEMORY || xrtErrorKind(error)==XERR_INTERNAL);
        }else assert(xrtFutureState(activeOutput)==XFUTURE_RESOLVED && !error);
    }else check_outcome();
    assert(count(&firstDrops) == 1 && count(&unusedDrops) == 1);
    if (!scanFault && (kind == 3 || kind == 4) && !bodyMode && !boxFault)
        assert(count(&resultDrops) == (expectedState == XFUTURE_RESOLVED ? 0u : 1u));
    assert(xrtAtomic32FetchAdd(&publications, 1, XMEMORY_SEQ_CST) == 0); assert(xrtSemPost(observed));
}
static bool scan_case(unsigned selected,unsigned budget)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);worker_completion_prepare();
    kind=selected;bodyMode=firstMode=unusedMode=resultMode=cancelPoint=boxFault=0;
    scanBudget=budget;scanFault=true;xrtAtomic32Store(&scanTriggered,0,XMEMORY_SEQ_CST);
    xatomic32* counters[]={&entries,&bodies,&firstDrops,&unusedDrops,&resultDrops,&publications,&watchDrops,&boxFailures};
    for(size_t i=0;i<sizeof(counters)/sizeof(*counters);++i)xrtAtomic32Store(counters[i],0,XMEMORY_SEQ_CST);
    activeOutput=worker_completion_launch(kind,0);assert(activeOutput);
    assert(xrtSemWaitFor(entered,15000000)==XWAIT_OK && !xrtFutureDone(activeOutput));
    xfuturewatch watch;assert(xrtFutureWatchInit(&watch,observe,release_watch,NULL));
    assert(xrtFutureWatchAdd(activeOutput,&watch)==XFUTURE_WATCH_PENDING && xrtSemPost(proceed));
    assert(xrtFutureWaitFor(activeOutput,15000000)==XWAIT_OK && xrtSemWaitFor(observed,15000000)==XWAIT_OK);
    assert(xrtSemWaitFor(watchFinished,15000000)==XWAIT_OK);
    bool hit=count(&scanTriggered)!=0;
    if(!hit)worker_completion_check(kind,activeOutput);
    xrtFutureDestroy(activeOutput);activeOutput=NULL;xrtClearError();
    /* Module quiescence joins the actual worker, including payload/job frees.
     * There is no open-pool Wait substitute or additional observer owner. */
    worker_completion_finish();scanFault=false;
    assert(count(&firstDrops)==1 && count(&unusedDrops)==1 && count(&publications)==1 && count(&watchDrops)==1);
    assert(count(&resultDrops)<=1); /* Failed construction may never publish an owner. */
    if(!hit && (kind==3 || kind==4))assert(count(&resultDrops)==1);
    assert(!xrtGetError());continuation_owner_fault_balanced(&before);
    assert(xrtSemWaitFor(inputsFinished,0)==XWAIT_OK && xrtSemWaitFor(inputsFinished,0)==XWAIT_OK);
    if(count(&resultDrops))assert(xrtSemWaitFor(resultFinished,0)==XWAIT_OK);
    ++scanRuns;if(hit)++scanHits;
    printf("worker prefix kind=%u budget=%u hit=%u body=%u resultDrops=%u\n",kind,budget,(unsigned)hit,count(&bodies),count(&resultDrops));
    return hit;
}
static void scan_prefixes(void)
{
    for(unsigned selected=0;selected<6;++selected)if(selected!=1){
        bool complete=false;
        for(unsigned budget=0;budget<128;++budget)if(!scan_case(selected,budget)){complete=true;break;}
        assert(complete);
    }
    assert(scanRuns==scanHits+5);
    printf("worker allocation prefixes: 5 complete lanes, %u runs, %u real worker TLS failures, module joins and exact memory balance\n",scanRuns,scanHits);
}
static void release_watch(void* data)
{
    (void)data; assert(xrtAtomic32FetchAdd(&watchDrops, 1, XMEMORY_SEQ_CST) == 0);
    assert(xrtSemPost(watchFinished));
}
static void expect_failure(int mode, int code)
{
    expectedState = XFUTURE_FAILED; expectedKind = mode == 1 ? XERR_STATE : XERR_INTERNAL;
    expectedCode = mode == 1 ? code : 0;
}
static void run_case(void)
{
    xfuturewatch watch; xerror* ambient;
    xatomic32* counters[] = {&entries,&bodies,&firstDrops,&unusedDrops,&resultDrops,&publications,&watchDrops,&boxFailures};
    size_t i;
    for (i = 0; i < sizeof(counters)/sizeof(*counters); ++i) xrtAtomic32Store(counters[i], 0, XMEMORY_SEQ_CST);
    expectedState = XFUTURE_RESOLVED; expectedKind = XERR_NONE; expectedCode = 0;
    if (boxFault) { expectedState = XFUTURE_FAILED; expectedKind = XERR_MEMORY; }
    else if (bodyMode) expect_failure(bodyMode, 31);
    else if (cancelPoint == 1) expectedState = XFUTURE_CANCELLED;
    else if (unusedMode) expect_failure(unusedMode, 99);
    else if (firstMode) expect_failure(firstMode, 97);
    ambient = xrtErrorCreate(XERR_STATE, "worker-completion", 11, "caller diagnostic"); assert(ambient);
    xrtSetErrorTake(xrtErrorRef(ambient)); activeOutput = worker_completion_launch(kind, bodyMode);
    assert(xrtGetError() == ambient); xrtClearError();
    assert(xrtSemWaitFor(entered, 15000000) == XWAIT_OK && !xrtFutureDone(activeOutput));
    assert(xrtFutureWatchInit(&watch, observe, release_watch, NULL));
    assert(xrtFutureWatchAdd(activeOutput, &watch) == XFUTURE_WATCH_PENDING);
    if (cancelPoint == 1) assert(xrtFutureCancel(activeOutput));
    assert(xrtSemPost(proceed));
    if (cancelPoint == 2) {
        assert(xrtSemWaitFor(dropEntered, 15000000) == XWAIT_OK && !xrtFutureDone(activeOutput));
        assert(xrtFutureCancel(activeOutput)); assert(xrtSemPost(dropProceed)); ++lateCancels;
    }
    assert(xrtFutureWaitFor(activeOutput, 15000000) == XWAIT_OK);
    assert(xrtSemWaitFor(observed, 15000000) == XWAIT_OK); check_outcome();
    if (expectedState == XFUTURE_RESOLVED) worker_completion_check(kind, activeOutput);
    assert(xrtSemWaitFor(watchFinished, 15000000) == XWAIT_OK);
    assert(!xrtFutureCancel(activeOutput)); xrtFutureDestroy(activeOutput); activeOutput = NULL;
    // A successful managed result may raise when its final Future owner is
    // released. That is not a retroactive change to the published outcome.
    xrtClearError();
    if ((kind == 3 || kind == 4) && !bodyMode && !boxFault)
        assert(xrtSemWaitFor(resultFinished, 15000000) == XWAIT_OK);
    for (i = 0; i < 2; ++i) assert(xrtSemWaitFor(inputsFinished, 15000000) == XWAIT_OK);
    assert(count(&bodies) == (boxFault ? 0u : 1u) && count(&boxFailures) == (unsigned)boxFault);
    assert(count(&firstDrops) == 1 && count(&unusedDrops) == 1 && count(&publications) == 1 && count(&watchDrops) == 1);
    assert(count(&resultDrops) == ((kind == 3 || kind == 4) && !bodyMode && !boxFault ? 1u : 0u));
    xrtErrorFree(ambient); assert(!xrtGetError()); ++executions; ++observations; if (boxFault) ++faultExecutions;
}
static void run_discarded(void)
{
    xfuture* output; size_t i;
    xatomic32* counters[] = {&entries,&bodies,&firstDrops,&unusedDrops,&resultDrops,&publications,&watchDrops,&boxFailures};
    for (i = 0; i < sizeof(counters)/sizeof(*counters); ++i) xrtAtomic32Store(counters[i], 0, XMEMORY_SEQ_CST);
    assert(!cancelPoint && !boxFault && !activeOutput);
    output = worker_completion_launch(kind, bodyMode);
    assert(xrtSemWaitFor(entered, 15000000) == XWAIT_OK && !xrtFutureDone(output));
    // No observer or host reference keeps the public Future/result alive.
    // Transport ownership must finish the activation and dispose its payload.
    xrtFutureDestroy(output); assert(!xrtGetError()); assert(xrtSemPost(proceed));
    for (i = 0; i < 2; ++i) assert(xrtSemWaitFor(inputsFinished, 15000000) == XWAIT_OK);
    if ((kind == 3 || kind == 4) && !bodyMode)
        assert(xrtSemWaitFor(resultFinished, 15000000) == XWAIT_OK);
    assert(count(&bodies) == 1 && count(&firstDrops) == 1 && count(&unusedDrops) == 1);
    assert(count(&resultDrops) == ((kind == 3 || kind == 4) && !bodyMode ? 1u : 0u));
    assert(count(&publications) == 0 && count(&watchDrops) == 0 && !xrtGetError());
    ++discarded; ++executions;
}
static void cycle(void)
{
    worker_completion_prepare();
    for (kind = 0; kind < 6; ++kind)
    for (bodyMode = 0; bodyMode < 3; ++bodyMode)
    for (firstMode = 0; firstMode < 3; ++firstMode)
    for (unusedMode = 0; unusedMode < 3; ++unusedMode)
    for (resultMode = 0; resultMode < (smoke ? 1 : 3); ++resultMode)
    for (cancelPoint = 0; cancelPoint < 3; ++cancelPoint) { boxFault = 0; run_case(); }
    if (!smoke) for (kind = 0; kind < 6; ++kind) if (kind != 1)
    for (firstMode = 0; firstMode < 3; ++firstMode)
    for (unusedMode = 0; unusedMode < 3; ++unusedMode) {
        bodyMode = resultMode = cancelPoint = 0; boxFault = 1; run_case();
    }
    boxFault = cancelPoint = 0;
    for (kind = 0; kind < 6; ++kind)
    for (bodyMode = 0; bodyMode < 3; ++bodyMode)
    for (firstMode = 0; firstMode < 3; ++firstMode)
    for (unusedMode = 0; unusedMode < 3; ++unusedMode)
    for (resultMode = 0; resultMode < (smoke ? 1 : 3); ++resultMode) run_discarded();
    // Only module quiescence joins the worker threads and all task job frees.
    // An open pool's Wait API is not an idle barrier. Memory is measured after
    // this join, while each case separately awaits all its test callbacks.
    boxFault = 0; worker_completion_finish();
}
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after; unsigned beforeRuns, beforeFaults;
    smoke = argc == 2 && !strcmp(argv[1], "--smoke"); assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    mainThread = xrtThreadCurrentId();
    if (!smoke) xrtMemDebugSnapshot(&initial);
    entered = xrtSemCreate(0, 1); proceed = xrtSemCreate(0, 1); dropEntered = xrtSemCreate(0, 1);
    dropProceed = xrtSemCreate(0, 1); observed = xrtSemCreate(0, 1); assert(entered && proceed && dropEntered && dropProceed && observed);
    watchFinished = xrtSemCreate(0, 1); resultFinished = xrtSemCreate(0, 1); assert(watchFinished && resultFinished);
    inputsFinished = xrtSemCreate(0, 2); assert(inputsFinished);
    cycle(); beforeRuns = executions; beforeFaults = faultExecutions;
    if (!smoke) xrtMemDebugSnapshot(&before);
    cycle();
    if (!smoke) {
        continuation_owner_fault_balanced(&before); xrtMemDebugSnapshot(&after);
        assert(executions - beforeRuns == 1989 && faultExecutions - beforeFaults == 45 && discarded == 972);
        printf("async worker completion: %u executions, %u real worker result allocation failures, %u observers, %u destructor-time cancellations, %u discarded Futures; measured 1989-case cycle: %llu balanced allocations\n",
            executions, faultExecutions, observations, lateCancels, discarded, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else { assert(executions == 1296 && discarded == 324); printf("async worker completion C-host smoke: %u executions, %u discarded Futures\n", executions, discarded); }
    if(!smoke)scan_prefixes();
    xrtSemDestroy(entered); xrtSemDestroy(proceed); xrtSemDestroy(dropEntered); xrtSemDestroy(dropProceed); xrtSemDestroy(observed);
    xrtSemDestroy(watchFinished); xrtSemDestroy(resultFinished);
    xrtSemDestroy(inputsFinished);
    if (!smoke) continuation_owner_fault_balanced(&initial);
    return 0;
}
