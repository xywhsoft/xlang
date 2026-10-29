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
#include "xlir_owner_fault_runtime.h"
extern void cell_cancel_prepare(void), cell_cancel_finish(void);
extern xfuture* cell_cancel_launch(xfuture*, xfuture*, bool);
extern void cell_cancel_produce(xpromise*, int64_t*, int);
static xatomic32 phases[5], drops, caught, published, watchRetired, cancelAccepted, registrations;
static xsem *entered, *proceed, *start, *readyStart, *registered, *registrationReturn;
static xfuture *activeOutput, *cleanupSource;
static xpromise *firstPromise, *cleanupPromise;
static xerror* producerError;
static int point, dropMode, ambientMode, cleanupReady, smoke;
static unsigned executions, racingCases, independentCases, overlapCases, cancellations, handled, cancelled, lateSuccess;
static int64_t inputValue = 10, cleanupValue = 20;
static unsigned count(xatomic32* value) { return xrtAtomic32Load(value, XMEMORY_SEQ_CST); }
// The immutable sentinel is independently retained by each producer scope.
// Its root owner lives until every producer and watcher has finished.
const xerror* cell_cancel_producer_error(void) { return producerError; }
static void race_start(void)
{ assert(xrtSemPost(readyStart)); assert(xrtSemWaitFor(start, 15000000) == XWAIT_OK); }
void cell_cancel_watch_registered(xfuture* source, int status)
{
    if (point != -4 || source != cleanupSource) return;
    assert(status == XFUTURE_WATCH_PENDING && xrtAtomic32FetchAdd(&registrations, 1, XMEMORY_SEQ_CST) == 0);
    assert(xrtSemPost(registered) && xrtSemPost(registered));
    assert(xrtSemWaitFor(registrationReturn, 15000000) == XWAIT_OK);
    // The new activation must retire its owners before publication even
    // while the old activation's WatchAdd caller is still on another stack.
    assert(count(&drops) == 1 && count(&published) == 1);
}
void cell_cancel_phase(int64_t phase)
{
    assert(phase >= 1 && phase <= 4);
    assert(xrtAtomic32FetchAdd(&phases[phase], 1, XMEMORY_SEQ_CST) == 0);
    if (point == phase) {
        assert(activeOutput && !xrtFutureDone(activeOutput));
        assert(xrtSemPost(entered));
        assert(xrtSemWaitFor(proceed, 15000000) == XWAIT_OK);
    }
}
void cell_cancel_caught(void) { assert(xrtAtomic32FetchAdd(&caught, 1, XMEMORY_SEQ_CST) == 0); }
int cell_cancel_dropped(void)
{
    assert(xrtAtomic32FetchAdd(&drops, 1, XMEMORY_SEQ_CST) == 0);
    assert(activeOutput && !xrtFutureDone(activeOutput)); cell_cancel_phase(4); return dropMode;
}
xfuture* cell_cancel_input(void) { assert(0 && "reachability helper must not execute"); return NULL; }
static void observe(void* data)
{
    (void)data;
    // A raw XRT watcher may observe the restored producer diagnostic, but
    // never the language continuation's cancellation or destructor failure.
    int foreignError = xrtGetError() && xrtGetError() != cell_cancel_producer_error();
    if (count(&drops) != 1 || foreignError) fprintf(stderr,
        "cell cancel observer: point=%d ready=%d dropMode=%d ambient=%d drops=%u caught=%u error=%d phases=%u/%u/%u/%u\n",
        point, cleanupReady, dropMode, ambientMode, count(&drops), count(&caught), xrtGetError() ? xrtErrorCode(xrtGetError()) : 0,
        count(&phases[1]), count(&phases[2]), count(&phases[3]), count(&phases[4]));
    assert(count(&drops) == 1 && !foreignError);
    assert(xrtAtomic32FetchAdd(&published, 1, XMEMORY_SEQ_CST) == 0);
}
static void release_watch(void* data)
{ (void)data; assert(xrtAtomic32FetchAdd(&watchRetired, 1, XMEMORY_SEQ_CST) == 0); }
static int32 resolve_thread(void* data)
{
    (void)data;
    if (point == -2 || point == -3) race_start();
    cell_cancel_produce(firstPromise, &inputValue, ambientMode);
    if (!cleanupReady && point >= -2) cell_cancel_produce(cleanupPromise, &cleanupValue, ambientMode);
    assert(!xrtGetError()); return 0;
}
static int32 cancel_thread(void* data)
{
    (void)data;
    if (point == -4) assert(xrtSemWaitFor(registered, 15000000) == XWAIT_OK);
    race_start();
    xrtAtomic32Store(&cancelAccepted, xrtFutureCancel(activeOutput) ? 1 : 0, XMEMORY_SEQ_CST);
    assert(!xrtGetError()); return 0;
}
static int32 cleanup_thread(void* data)
{
    (void)data;
    if (point == -4) assert(xrtSemWaitFor(registered, 15000000) == XWAIT_OK);
    race_start(); cell_cancel_produce(cleanupPromise, &cleanupValue, ambientMode);
    if (point == -4) { assert(count(&drops) == 1 && count(&published) == 1); assert(xrtSemPost(registrationReturn)); }
    assert(!xrtGetError()); return 0;
}
static void request_cancel(void)
{
    assert(xrtFutureCancel(activeOutput)); xrtAtomic32Store(&cancelAccepted, 1, XMEMORY_SEQ_CST);
    assert(!xrtFutureCancel(activeOutput) && !xrtFutureDone(activeOutput) && !xrtGetError());
}
static void check_outcome(const xfutureresult* result, int handle)
{
    unsigned seen = count(&caught); int cancellationWins = 0, expected = seen ? 107 : 17;
    assert(seen <= 1 && count(&drops) == 1 && count(&phases[2]) == 1 && count(&phases[4]) == 1);
    if (point >= -1) {
        assert(seen == (unsigned)(point == 0));
        cancellationWins = (point == 0 && !handle) || point == 1 || point == 2;
        assert(count(&phases[1]) == (unsigned)(point != 0));
        assert(count(&phases[3]) == (unsigned)(point != 1 && point != 2));
    } else {
        if (seen) assert(count(&cancelAccepted));
        if (!count(&cancelAccepted)) assert(!seen);
        assert(count(&phases[1]) == 1u - seen);
        cancellationWins = (seen && !handle) || count(&phases[3]) == 0;
    }
    if (cancellationWins) {
        assert(result->State == XFUTURE_CANCELLED && !result->Value && !result->Error); ++cancelled;
    } else if (dropMode) {
        assert(result->State == XFUTURE_FAILED && !result->Value && result->Error);
        if (dropMode == 1) assert(xrtErrorCode(result->Error) == 91);
        else assert(xrtErrorKind(result->Error) == XERR_INTERNAL);
    } else {
        assert(result->State == XFUTURE_RESOLVED && result->Value && !result->Error && *(int64_t*)result->Value == expected);
        if (seen) ++handled;
        else if (count(&cancelAccepted)) ++lateSuccess;
    }
}
static void run_case(int phase, int ready, int handle, int raises, int ambient)
{
    xmemdebugsnapshot before; xfuture *input = NULL, *cleanup = NULL, *result;
    xthread *resolver, *canceller = NULL, *cleanupWorker = NULL; xfuturewatch watch; xfutureresult outcome; unsigned i;
    if (!smoke) xrtMemDebugSnapshot(&before);
    point = phase; cleanupReady = ready; dropMode = raises; ambientMode = ambient;
    producerError = ambient == 1 ? xrtErrorCreate(XERR_STATE, "producer", 31, "unrelated failure") : NULL;
    assert(ambient != 1 || producerError);
    for (i = 0; i < 5; ++i) xrtAtomic32Init(&phases[i], 0);
    xrtAtomic32Init(&drops, 0); xrtAtomic32Init(&caught, 0); xrtAtomic32Init(&published, 0);
    xrtAtomic32Init(&watchRetired, 0); xrtAtomic32Init(&cancelAccepted, 0);
    xrtAtomic32Init(&registrations, 0);
    firstPromise = xrtPromiseCreate(&input, NULL); cleanupPromise = xrtPromiseCreate(&cleanup, NULL);
    assert(firstPromise && cleanupPromise && input && cleanup);
    cleanupSource = cleanup;
    if (ready) cell_cancel_produce(cleanupPromise, &cleanupValue, 0);
    result = cell_cancel_launch(input, cleanup, handle != 0); assert(!xrtFutureDone(result)); activeOutput = result;
    assert(xrtFutureWatchInit(&watch, observe, release_watch, NULL));
    assert(xrtFutureWatchAdd(result, &watch) == XFUTURE_WATCH_PENDING);
    if (point == 0) request_cancel();
    resolver = xrtThreadCreate(resolve_thread, NULL, 0); assert(resolver);
    if (point <= -2) {
        unsigned workers = point == -3 ? 3u : 2u;
        canceller = xrtThreadCreate(cancel_thread, NULL, 0); assert(canceller);
        if (point <= -3) {
            assert(!ready); cleanupWorker = xrtThreadCreate(cleanup_thread, NULL, 0); assert(cleanupWorker);
            if (point == -3) ++independentCases;
            else ++overlapCases;
        }
        for (i = 0; i < workers; ++i) assert(xrtSemWaitFor(readyStart, 15000000) == XWAIT_OK);
        for (i = 0; i < workers; ++i) assert(xrtSemPost(start));
        if (point != -4) ++racingCases;
    } else if (point > 0) {
        assert(xrtSemWaitFor(entered, 15000000) == XWAIT_OK);
        assert(!count(&published) && !xrtFutureDone(result));
        request_cancel(); assert(xrtSemPost(proceed));
    }
    assert(xrtThreadWaitFor(resolver, 15000000) == XWAIT_OK); xrtThreadDestroy(resolver);
    if (canceller) { assert(xrtThreadWaitFor(canceller, 15000000) == XWAIT_OK); xrtThreadDestroy(canceller); }
    if (cleanupWorker) { assert(xrtThreadWaitFor(cleanupWorker, 15000000) == XWAIT_OK); xrtThreadDestroy(cleanupWorker); }
    assert(xrtFutureResult(result, &outcome)); check_outcome(&outcome, handle);
    assert(count(&published) == 1 && count(&watchRetired) == 1);
    assert(count(&registrations) == (unsigned)(point == -4));
    assert(xrtFutureState(input) == XFUTURE_RESOLVED && xrtFutureState(cleanup) == XFUTURE_RESOLVED);
    assert(!xrtFutureCancel(result) && !xrtGetError()); cancellations += count(&cancelAccepted);
    xrtFutureWatchRemove(result, &watch); assert(count(&watchRetired) == 1);
    activeOutput = NULL; cleanupSource = NULL; xrtFutureDestroy(result);
    xrtPromiseDestroy(firstPromise); xrtPromiseDestroy(cleanupPromise); xrtFutureDestroy(input); xrtFutureDestroy(cleanup);
    xrtErrorFree(producerError); producerError = NULL;
    assert(!xrtGetError()); if (!smoke) continuation_owner_fault_balanced(&before); ++executions;
}
int main(int argc, char** argv)
{
    xmemdebugsnapshot before, after; int round, phase, ready, handle, raises, ambient;
    smoke = argc == 2 && !strcmp(argv[1], "--smoke"); assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&before);
    cell_cancel_prepare(); entered = xrtSemCreate(0, 1); proceed = xrtSemCreate(0, 1);
    start = xrtSemCreate(0, 3); readyStart = xrtSemCreate(0, 3);
    registered = xrtSemCreate(0, 2); registrationReturn = xrtSemCreate(0, 1);
    assert(entered && proceed && start && readyStart && registered && registrationReturn);
    for (phase = -4; phase <= 4; ++phase)
    for (round = 0; round < (smoke ? 1 : phase <= -2 ? 20 : 12); ++round)
    for (ready = 0; ready < 2; ++ready) for (handle = 0; handle < 2; ++handle)
    for (raises = 0; raises < 3; ++raises) for (ambient = 0; ambient < 3; ++ambient)
        if (phase >= -2 || !ready) run_case(phase, ready, handle, raises, ambient);
    xrtSemDestroy(entered); xrtSemDestroy(proceed); xrtSemDestroy(start); xrtSemDestroy(readyStart);
    xrtSemDestroy(registered); xrtSemDestroy(registrationReturn); cell_cancel_finish();
    assert(cancelled && handled && lateSuccess);
    if (!smoke) {
        assert(executions == 4032 && racingCases == 1080 && independentCases == 360 && overlapCases == 360);
        continuation_owner_fault_balanced(&before); xrtMemDebugSnapshot(&after);
        printf("cell cancellation races: %u executions (%u unforced races, %u independent-producer races, %u forced overlapping frames), %u accepted requests, %u handled, %u cancelled, %u late successes, %llu balanced allocations\n",
            executions, racingCases, independentCases, overlapCases, cancellations, handled, cancelled, lateSuccess, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else {
        assert(executions == 288 && racingCases == 54 && independentCases == 18 && overlapCases == 18);
        printf("cell cancellation C-host smoke: %u executions (%u races, %u overlapping frames); once-only delivery, catch/finally, cleanup barrier and producer isolation\n", executions, racingCases, overlapCases);
    }
    return 0;
}
