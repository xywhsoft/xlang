#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_async_source_sync.h"
extern int terminal_host_init(void);
extern void terminal_host_fini(void);
extern xfuture* terminal_host_start(int, xfuture*, xfuture*);
extern xfuture* terminal_host_return(bool, xfuture*);
extern int64_t terminal_sync(void);
static xerror *primary, *secondary, *cleanup;
static xfuture* syncInput;
static unsigned finalCount, caughtCount, afterCount;
static int raising;
static int64_t payload = 10;
void terminal_finally(void) { ++finalCount; if (raising) xrtSetError(cleanup); }
void terminal_caught(void) { ++caughtCount; }
void terminal_after(void) { ++afterCount; }
xfuture* terminal_input(void)
{
    xfuture* result = NULL; xpromise* promise;
    if (syncInput) return xrtFutureRef(syncInput);
    promise = xrtPromiseCreate(&result, NULL);
    assert(promise && result && xrtPromiseResolve(promise, &payload));
    xrtPromiseDestroy(promise); return result;
}
static void complete(xpromise* promise, unsigned mode, xerror* error)
{
    if (mode == 0) assert(xrtPromiseResolve(promise, &payload));
    else if (mode == 1) assert(xrtPromiseReject(promise, error));
    else if (mode == 2) assert(xrtPromiseCancel(promise));
    else assert(xrtPromiseClose(promise));
}
static void balanced(const xmemdebugsnapshot* before)
{
    xlir_async_test_wait_retired();
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    if (before->LiveCount != after.LiveCount || before->LiveBytes != after.LiveBytes)
        fprintf(stderr, "terminal live: %llu/%llu -> %llu/%llu\n", (unsigned long long)before->LiveCount,
            (unsigned long long)before->LiveBytes, (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
int main(int argc, char** argv)
{
    unsigned round, kind, mode, next, raiseFinally, executions = 0, suspensions = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    xmemdebugsnapshot initial, before, final;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    primary = xrtErrorCreate(XERR_STATE, "terminal", 88, "input failure");
    secondary = xrtErrorCreate(XERR_STATE, "terminal", 99, "second input failure");
    cleanup = xrtErrorCreate(XERR_STATE, "terminal", 77, "finally failure");
    assert(primary && secondary && cleanup && terminal_host_init());
    assert(terminal_sync() == 10 && !xrtGetError());
    for (round = 0; round < (smoke ? 1u : 25u); ++round)
    for (kind = 0; kind < 6; ++kind)
    for (mode = 0; mode < 4; ++mode)
    for (next = 0; next < (kind == 3 ? 4u : 1u); ++next)
    for (raiseFinally = 0; raiseFinally < 2; ++raiseFinally) {
        xfuture *input = NULL, *second = NULL, *result;
        xpromise *firstPromise, *secondPromise;
        const xerror* error;
        unsigned effective = mode, expectedFinally = kind == 3 && next ? 0u : 1u;
        int expectedCode = mode == 1 ? 88 : 0;
        int64_t expectedValue = 10;
        if (!smoke) xrtMemDebugSnapshot(&before);
        finalCount = caughtCount = afterCount = 0; raising = (int)raiseFinally;
        firstPromise = xrtPromiseCreate(&input, NULL); secondPromise = xrtPromiseCreate(&second, NULL);
        assert(firstPromise && input && secondPromise && second);
        result = terminal_host_start((int)kind, input, second);
        assert(result && !xrtFutureDone(result) && !finalCount && !caughtCount && !afterCount && !xrtGetError());
        ++suspensions;
        complete(firstPromise, mode, primary); xrtPromiseDestroy(firstPromise);
        if (kind == 3) { assert(!xrtFutureDone(result)); ++suspensions; }
        complete(secondPromise, next, secondary); xrtPromiseDestroy(secondPromise);
        assert(xrtFutureWait(result) == 0);
        if (kind == 1 && mode) { effective = 0; expectedCode = 0; expectedValue = mode == 2 ? 17 : 19; }
        /* Explicit source operations in finally override a pending exception;
         * only automatic cleanup preserves it. Await failure and this foreign
         * finally call are explicit operations, not destructor cleanup. */
        if (kind == 3 && next) { effective = next; expectedCode = next == 1 ? 99 : 0; }
        if (kind == 5 && !raiseFinally) { effective = 0; expectedCode = 0; expectedValue = 37; }
        if (raiseFinally && expectedFinally) { effective = 1; expectedCode = 77; }
        error = xrtFutureError(result);
        if (round == 0) fprintf(stderr, "terminal kind=%u input=%u second=%u raising=%u state=%d code=%d finally=%u caught=%u after=%u\n",
            kind, mode, next, raiseFinally, (int)xrtFutureState(result), error ? xrtErrorCode(error) : 0, finalCount, caughtCount, afterCount);
        assert(xrtFutureState(result) == (effective == 0 ? XFUTURE_RESOLVED : effective == 1 ? XFUTURE_FAILED : effective == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED));
        assert(expectedCode ? error && xrtErrorCode(error) == expectedCode : error == NULL);
        if (effective == 0) { const int64_t* value = (const int64_t*)xrtFutureValue(result); assert(value && *value == expectedValue); }
        assert(finalCount == expectedFinally && caughtCount == ((kind == 1 || kind == 2) && mode ? 1u : 0u));
        assert(afterCount == (mode == 0 && kind != 4 ? 1u : 0u));
        xrtFutureDestroy(result); xrtFutureDestroy(input); xrtFutureDestroy(second);
        assert(!xrtGetError()); if (!smoke) balanced(&before); ++executions;
    }
    /* All branches return: no reachable fallthrough is synthesized after the
     * try/finally. Both suspended return values obey the same exit protocol. */
    for (round = 0; round < (smoke ? 1u : 25u); ++round)
    for (kind = 0; kind < 2; ++kind)
    for (mode = 0; mode < 4; ++mode)
    for (raiseFinally = 0; raiseFinally < 2; ++raiseFinally) {
        xfuture *input = NULL, *result;
        xpromise* promise;
        const xerror* error;
        unsigned effective = raiseFinally ? 1u : mode;
        int expectedCode = raiseFinally ? 77 : mode == 1 ? 88 : 0;
        if (!smoke) xrtMemDebugSnapshot(&before);
        finalCount = caughtCount = afterCount = 0; raising = (int)raiseFinally;
        promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
        result = terminal_host_return(kind != 0, input);
        assert(result && !xrtFutureDone(result) && !finalCount && !xrtGetError());
        complete(promise, mode, primary); xrtPromiseDestroy(promise); ++suspensions;
        assert(xrtFutureWait(result) == 0);
        assert(xrtFutureState(result) == (effective == 0 ? XFUTURE_RESOLVED : effective == 1 ? XFUTURE_FAILED : effective == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED));
        error = xrtFutureError(result);
        assert(expectedCode ? error && xrtErrorCode(error) == expectedCode : error == NULL);
        if (effective == 0) { const int64_t* value = (const int64_t*)xrtFutureValue(result); assert(value && *value == (kind ? 10 : 11)); }
        assert(finalCount == 1 && !caughtCount && !afterCount && !xrtGetError());
        xrtFutureDestroy(result); xrtFutureDestroy(input);
        assert(!xrtGetError()); if (!smoke) balanced(&before); ++executions;
    }
    raising = 0;
    for (mode = 0; mode < 4; ++mode) {
        xpromise* promise;
        terminal_host_fini(); assert(!xrtGetError());
        promise = xrtPromiseCreate(&syncInput, NULL); assert(promise && syncInput);
        complete(promise, mode, primary); xrtPromiseDestroy(promise);
        finalCount = caughtCount = afterCount = 0;
        assert(terminal_host_init() && terminal_sync() == (mode == 0 ? 10 : mode == 2 ? 17 : 19));
        assert(finalCount == 1 && caughtCount == (mode ? 1u : 0u) && !xrtGetError());
        xrtFutureDestroy(syncInput); syncInput = NULL;
    }
    terminal_host_fini(); assert(!xrtGetError());
    xrtErrorFree(primary); xrtErrorFree(secondary); xrtErrorFree(cleanup);
    if (!smoke) { balanced(&initial); xrtMemDebugSnapshot(&final);
        printf("async terminal outcomes: %u executions, %u real suspensions, %llu balanced allocations\n", executions, suspensions,
            (unsigned long long)(final.AllocCount - initial.AllocCount));
    } else puts("async terminal outcomes: generated C-host smoke passed");
    return 0;
}
