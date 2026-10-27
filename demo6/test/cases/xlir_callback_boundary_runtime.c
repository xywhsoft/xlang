#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_SEM
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int callback_boundary_init(void);
extern void callback_boundary_fini(void);
extern void callback_boundary_wait_idle(void);
extern xfuture* callback_boundary_start(unsigned, xfuture*);
static xatomic32 drops = {0}, calls = {0}, publications = {0};
static xatomic32 dispatchFaultArmed = {0}, dispatchFaultTriggered = {0};
static uint64_t dispatchFaultBudget;
static int dispatchFaultActive;
static unsigned payloadDrops;
static unsigned currentKind, currentMode, currentErrors;
static int raiseDrop, raiseCall, taskMode, blockDrop;
static xerror *cleanupError, *callError, *inputError, *sentinel;
static xsem *entered, *proceed;
static unsigned count(xatomic32* value) { return xrtAtomic32Load(value, XMEMORY_SEQ_CST); }
void* callback_boundary_dispatch_malloc(size_t bytes)
{
    if (xrtAtomic32Exchange(&dispatchFaultArmed, 0, XMEMORY_SEQ_CST)) {
        assert(!dispatchFaultActive && xrtMemDebugFailAfter(dispatchFaultBudget));
        dispatchFaultActive = 1;
    }
    return xrtMalloc(bytes);
}
void callback_boundary_drop(void)
{
    /* Capture cleanup always runs on the invoking worker, even if allocation
     * failed before user code. Clear its TLS fault before publishing the
     * outcome, so it cannot leak into another job on the fixed executor. */
    if (dispatchFaultActive) {
        xrtAtomic32Store(&dispatchFaultTriggered, xrtMemDebugFailTriggered(), XMEMORY_SEQ_CST);
        xrtMemDebugFailClear(); dispatchFaultActive = 0;
    }
    xrtAtomic32FetchAdd(&drops, 1, XMEMORY_SEQ_CST);
    if (blockDrop) {
        assert(xrtSemPost(entered));
        assert(xrtSemWaitFor(proceed, 15000000) == XWAIT_OK);
    }
    if (raiseDrop) xrtSetError(cleanupError);
}
int64_t callback_boundary_work(int64_t value)
{
    xrtAtomic32FetchAdd(&calls, 1, XMEMORY_SEQ_CST);
    if (taskMode) {
        assert(xrtSemPost(entered));
        assert(xrtSemWaitFor(proceed, 15000000) == XWAIT_OK);
    }
    if (raiseCall) xrtSetError(callError);
    return value;
}
void callback_boundary_finally(int64_t value) { (void)callback_boundary_work(value); }
static int64_t placeholder = 10;
static int32 resolve_on_thread(void* data)
{
    assert(xrtPromiseResolve((xpromise*)data, &placeholder));
    assert(!xrtGetError()); return 0;
}
xfuture* callback_boundary_input(void)
{
    xfuture* f = NULL; xpromise* p = xrtPromiseCreate(&f, NULL);
    assert(p && xrtPromiseResolve(p, &placeholder)); xrtPromiseDestroy(p); return f;
}
static void payload_free(void* value, void* data)
{
    (void)data; ++payloadDrops; xrtFree(value);
}
static void published(void* data)
{
    unsigned expected = *(unsigned*)data;
    if (count(&drops) != expected || (xrtGetError() && xrtGetError() != sentinel)) fprintf(stderr,
        "callback publication kind=%u mode=%u errors=%u drops=%u/%u thread_error=%d\n",
        currentKind, currentMode, currentErrors, count(&drops), expected,
        xrtGetError() ? xrtErrorCode(xrtGetError()) : 0);
    assert(count(&drops) == expected);
    /* Raw XRT observers may run after the language callback has returned and
     * restored the host context. They must never see its cleanup/call error. */
    assert(!xrtGetError() || xrtGetError() == sentinel);
    xrtAtomic32FetchAdd(&publications, 1, XMEMORY_SEQ_CST);
}
static void watch_free(void* data) { (void)data; }
static void complete(xpromise* promise, unsigned mode, int64_t* payload)
{
    if (mode == 0) assert(xrtPromiseResolveOwned(promise, payload, payload_free, NULL));
    else if (mode == 1) assert(xrtPromiseReject(promise, inputError));
    else if (mode == 2) assert(xrtPromiseCancel(promise));
    else assert(xrtPromiseClose(promise));
}
static void balanced(const xmemdebugsnapshot* before)
{
    callback_boundary_wait_idle();
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
int main(int argc, char** argv)
{
    unsigned round, kind, mode, errors, executions = 0, cancelled = 0, faults = 0;
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    xmemdebugsnapshot initial, before, final;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    cleanupError = xrtErrorCreate(XERR_STATE, "callback", 77, "capture destructor failed");
    callError = xrtErrorCreate(XERR_STATE, "callback", 88, "callback failed");
    inputError = xrtErrorCreate(XERR_STATE, "callback", 66, "source failed");
    sentinel = xrtErrorCreate(XERR_STATE, "callback", 99, "host context");
    entered = xrtSemCreate(0, 1); proceed = xrtSemCreate(0, 1);
    assert(cleanupError && callError && inputError && sentinel && entered && proceed);
    assert(callback_boundary_init());
    for (round = 0; round < (smoke ? 1u : 20u); ++round)
    for (kind = 0; kind < 3; ++kind)
    for (mode = 0; mode < 4; ++mode)
    for (errors = 0; errors < 4; ++errors) {
        xfuture *source = NULL, *result, *alias;
        xpromise* promise;
        xfuturewatch watch;
        int64_t* payload = NULL;
        unsigned dropped = count(&drops), called = count(&calls), publishedCount = count(&publications);
        unsigned expectedDrop = dropped + 1, sourceDrops = payloadDrops;
        int invokes = kind == 2 || (kind == 0 && mode == 0) || (kind == 1 && mode == 1);
        int forward = kind == 2 || !invokes;
        int expectedCode = invokes && (errors & 2) ? 88
            : forward && mode != 0 ? (mode == 1 ? 66 : 0)
            : (errors & 1) ? 77 : 0;
        xfuturestate expected = expectedCode ? XFUTURE_FAILED
            : forward && mode == 2 ? XFUTURE_CANCELLED
            : forward && mode == 3 ? XFUTURE_CLOSED : XFUTURE_RESOLVED;
        if (!smoke) xrtMemDebugSnapshot(&before);
        raiseDrop = (errors & 1) != 0; raiseCall = (errors & 2) != 0;
        currentKind = kind; currentMode = mode; currentErrors = errors;
        promise = xrtPromiseCreate(&source, NULL); assert(promise);
        if (mode == 0) { payload = xrtMalloc(sizeof(*payload)); assert(payload); *payload = 10; }
        result = callback_boundary_start(kind, source);
        assert(result && !xrtFutureDone(result) && count(&drops) == dropped && !xrtGetError());
        assert(xrtFutureWatchInit(&watch, published, watch_free, &expectedDrop));
        assert(xrtFutureWatchAdd(result, &watch) == XFUTURE_WATCH_PENDING);
        xrtSetError(sentinel);
        complete(promise, mode, payload);
        assert(xrtGetError() == sentinel); xrtClearError();
        xrtPromiseDestroy(promise);
        assert(xrtFutureWait(result) == XWAIT_OK); xrtFutureWatchRemove(result, &watch);
        assert(xrtFutureState(result) == expected && count(&drops) == expectedDrop);
        assert(count(&calls) == called + (invokes ? 1u : 0u));
        assert(count(&publications) == publishedCount + 1);
        if (expected == XFUTURE_FAILED) assert(xrtErrorCode(xrtFutureError(result)) == expectedCode);
        else assert(!xrtFutureError(result));
        if (expected == XFUTURE_RESOLVED) {
            assert(xrtFutureValue(result));
            assert(*(int64_t*)xrtFutureValue(result) == (forward ? 10 : kind == 0 ? 15 : 5));
            if (forward) assert(xrtFutureValue(result) == payload);
        }
        alias = xrtFutureRef(result); assert(alias); xrtFutureDestroy(source); xrtFutureDestroy(result);
        callback_boundary_wait_idle();
        if (mode == 0) assert(payloadDrops == sourceDrops + (forward && expected == XFUTURE_RESOLVED ? 0u : 1u));
        xrtFutureDestroy(alias);
        assert(payloadDrops == sourceDrops + (mode == 0 ? 1u : 0u) && !xrtGetError());
        if (!smoke) balanced(&before);
        ++executions;
    }
    /* A cancellation requested before source completion skips user code, but
     * still has to finish capture cleanup before notifying output observers. */
    for (kind = 0; kind < 3; ++kind)
    for (errors = 0; errors < 2; ++errors) {
        xfuture *source = NULL, *result; xfuturewatch watch;
        xpromise* promise = xrtPromiseCreate(&source, NULL);
        unsigned called = count(&calls), expectedDrop = count(&drops) + 1;
        raiseDrop = errors != 0; raiseCall = 0;
        currentKind = kind; currentMode = 4; currentErrors = errors;
        result = callback_boundary_start(kind, source); assert(result);
        assert(xrtFutureWatchInit(&watch, published, watch_free, &expectedDrop));
        assert(xrtFutureWatchAdd(result, &watch) == XFUTURE_WATCH_PENDING);
        assert(xrtFutureCancel(result));
        assert(xrtPromiseResolve(promise, &placeholder));
        assert(xrtFutureWait(result) == XWAIT_OK); xrtFutureWatchRemove(result, &watch);
        assert(xrtFutureState(result) == XFUTURE_CANCELLED && count(&calls) == called);
        assert(count(&drops) == expectedDrop && !xrtGetError());
        xrtPromiseDestroy(promise); xrtFutureDestroy(source); xrtFutureDestroy(result);
        ++cancelled;
    }
    for (mode = 0; mode < 2; ++mode) {
        xfuture *source = NULL, *result;
        xpromise* promise = xrtPromiseCreate(&source, NULL);
        xthread* resolver; int early;
        raiseDrop = 1; raiseCall = 0; blockDrop = 1;
        result = callback_boundary_start(0, source); assert(result);
        if (mode) assert(xrtFutureCancel(result));
        resolver = xrtThreadCreate(resolve_on_thread, promise, 0); assert(resolver);
        assert(xrtSemWaitFor(entered, 15000000) == XWAIT_OK);
        early = xrtFutureDone(result);
        assert(xrtSemPost(proceed));
        assert(xrtThreadWaitFor(resolver, 15000000) == XWAIT_OK); xrtThreadDestroy(resolver);
        fprintf(stderr, "callback publication barrier cancelled=%u published_during_cleanup=%d\n", mode, early);
        assert(!early);
        assert(xrtFutureWait(result) == XWAIT_OK);
        assert(xrtFutureState(result) == (mode ? XFUTURE_CANCELLED : XFUTURE_FAILED));
        xrtPromiseDestroy(promise); xrtFutureDestroy(source); xrtFutureDestroy(result);
        blockDrop = 0;
    }
    /* An owned callback result remains pending until capture cleanup commits;
     * rollback must destroy it once without replacing the primary error. */
    for (round = 0; round < (smoke ? 1u : 20u); ++round)
    for (errors = 0; errors < 4; ++errors) {
        xfuture *source = NULL, *result, *alias;
        xpromise* promise; xfuturewatch watch;
        unsigned firstDrop = count(&drops);
        unsigned expectedDrop = firstDrop + ((errors == 1) ? 2u : 1u);
        if (!smoke) xrtMemDebugSnapshot(&before);
        currentKind = 4; currentMode = 0; currentErrors = errors;
        raiseDrop = (errors & 1) != 0; raiseCall = (errors & 2) != 0;
        promise = xrtPromiseCreate(&source, NULL); assert(promise);
        result = callback_boundary_start(4, source); assert(result);
        assert(xrtFutureWatchInit(&watch, published, watch_free, &expectedDrop));
        assert(xrtFutureWatchAdd(result, &watch) == XFUTURE_WATCH_PENDING);
        assert(xrtPromiseResolve(promise, &placeholder)); xrtPromiseDestroy(promise);
        assert(xrtFutureWait(result) == XWAIT_OK); xrtFutureWatchRemove(result, &watch);
        assert(xrtFutureState(result) == (errors ? XFUTURE_FAILED : XFUTURE_RESOLVED));
        if (errors) assert(xrtErrorCode(xrtFutureError(result)) == ((errors & 2) ? 88 : 77));
        else assert(xrtFutureValue(result) && *(void**)xrtFutureValue(result));
        alias = xrtFutureRef(result); xrtFutureDestroy(source); xrtFutureDestroy(result);
        assert(count(&drops) == expectedDrop); xrtFutureDestroy(alias);
        callback_boundary_wait_idle();
        assert(count(&drops) == firstDrop + ((errors & 2) ? 1u : 2u) && !xrtGetError());
        if (!smoke) balanced(&before);
        ++executions;
    }
    if (!smoke) for (kind = 0; kind < 3; ++kind)
    for (mode = 0; mode < 28; ++mode) {
        xfuture *source = NULL, *result; xpromise* promise;
        unsigned firstCall = count(&calls); int triggered;
        raiseDrop = raiseCall = 0; xrtMemDebugSnapshot(&before);
        promise = xrtPromiseCreate(&source, NULL); assert(promise);
        assert(xrtMemDebugFailAfter(mode)); result = callback_boundary_start(kind, source);
        triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (!result) { assert(xrtGetError()); xrtClearError(); assert(count(&calls) == firstCall); }
        else assert(!xrtGetError());
        assert(xrtPromiseClose(promise)); xrtPromiseDestroy(promise);
        xrtFutureDestroy(result); xrtFutureDestroy(source); assert(!xrtGetError());
        balanced(&before); faults += triggered ? 1u : 0u;
    }
    /* Inject inside actual dispatch, including owned result construction.
     * A failed allocation cannot publish a default value as success. */
    if (!smoke) for (kind = 0; kind < 2; ++kind)
    for (mode = 0; mode < 20; ++mode) {
        xfuture *source = NULL, *result; xpromise* promise; int triggered;
        raiseDrop = raiseCall = 0; xrtMemDebugSnapshot(&before);
        promise = xrtPromiseCreate(&source, NULL); assert(promise);
        result = callback_boundary_start(kind ? 4 : 0, source); assert(result);
        dispatchFaultBudget = mode;
        xrtAtomic32Store(&dispatchFaultTriggered, 0, XMEMORY_SEQ_CST);
        xrtAtomic32Store(&dispatchFaultArmed, 1, XMEMORY_SEQ_CST);
        assert(xrtPromiseResolve(promise, &placeholder));
        assert(xrtFutureWait(result) == XWAIT_OK && !xrtGetError());
        callback_boundary_wait_idle();
        assert(!count(&dispatchFaultArmed) && !dispatchFaultActive);
        triggered = (int)count(&dispatchFaultTriggered);
        assert(xrtFutureState(result) == (triggered ? XFUTURE_FAILED : XFUTURE_RESOLVED));
        xrtPromiseDestroy(promise); xrtFutureDestroy(source); xrtFutureDestroy(result);
        assert(!xrtGetError()); balanced(&before); faults += triggered ? 1u : 0u;
    }
    taskMode = 1;
    for (round = 0; round < (smoke ? 1u : 20u); ++round)
    for (errors = 0; errors < 4; ++errors)
    for (mode = 0; mode < 2; ++mode) {
        xfuture* result; xfuturewatch watch;
        unsigned expectedDrop = count(&drops) + 1;
        int expectedCode = (errors & 2) ? 88 : (errors & 1) ? 77 : 0;
        int cancelWins = mode && !(errors & 2);
        if (!smoke) xrtMemDebugSnapshot(&before);
        raiseDrop = (errors & 1) != 0; raiseCall = (errors & 2) != 0;
        currentKind = 3; currentMode = mode; currentErrors = errors;
        result = callback_boundary_start(3, NULL); assert(result);
        assert(xrtSemWaitFor(entered, 15000000) == XWAIT_OK);
        assert(xrtFutureWatchInit(&watch, published, watch_free, &expectedDrop));
        assert(xrtFutureWatchAdd(result, &watch) == XFUTURE_WATCH_PENDING);
        if (mode) assert(xrtFutureCancel(result));
        assert(xrtSemPost(proceed));
        assert(xrtFutureWait(result) == XWAIT_OK); xrtFutureWatchRemove(result, &watch);
        assert(xrtFutureState(result) == (cancelWins ? XFUTURE_CANCELLED : expectedCode ? XFUTURE_FAILED : XFUTURE_RESOLVED));
        if (!cancelWins && expectedCode) {
            const xerror* error = xrtFutureError(result);
            assert(error && xrtErrorCause(error) == ((errors & 2) ? callError : cleanupError));
            assert(xrtErrorCode(xrtErrorCause(error)) == expectedCode);
        } else if (!cancelWins) assert(*(int64_t*)xrtFutureValue(result) == 15);
        xrtFutureDestroy(result); assert(!xrtGetError());
        /* Pool bookkeeping is allowed to outlive the published Future; module
         * quiescence below provides the allocation balance barrier. */
        ++executions;
    }
    callback_boundary_fini(); assert(!xrtGetError());
    xrtSemDestroy(entered); xrtSemDestroy(proceed);
    xrtErrorFree(cleanupError); xrtErrorFree(callError); xrtErrorFree(inputError); xrtErrorFree(sentinel);
    if (!smoke) {
        balanced(&initial); xrtMemDebugSnapshot(&final);
        printf("callback boundary runtime: %u executions, %u pre-callback cancellations, %u allocation faults, %llu balanced allocations\n",
            executions, cancelled, faults, (unsigned long long)(final.AllocCount - initial.AllocCount));
    } else puts("callback boundary generated C-host smoke passed");
    return 0;
}
