#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int match_async_host_init(void);
extern void match_async_host_fini(void);
extern xfuture* match_async_start(int64_t kind, xfuture* pending);
extern void match_async_keep(void);
static xerror *cleanupError, *inputError;
static unsigned dropped, after;
static int raising;
void match_async_drop(void) { ++dropped; if (raising) xrtSetError(cleanupError); }
void match_async_after(int64_t count) { assert(count == 5 || count == 20); ++after; }
xfuture* match_async_input(void)
{
    static int64_t value = 10;
    xfuture* result = NULL;
    xpromise* promise = xrtPromiseCreate(&result, NULL);
    assert(promise && result && xrtPromiseResolve(promise, &value));
    xrtPromiseDestroy(promise); return result;
}
int main(int argc, char** argv)
{
    unsigned round, kind, mode, destruct, suspensions = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    int64_t value = 10;
    xmemdebugsnapshot initial, before, final;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    cleanupError = xrtErrorCreate(XERR_STATE, "match-async", 77, "destructor failure");
    inputError = xrtErrorCreate(XERR_STATE, "match-async", 88, "await input failure");
    assert(cleanupError && inputError && match_async_host_init());
    for (round = 0; round < (smoke ? 1u : 25u); ++round)
    for (kind = 0; kind < 8; ++kind)
    for (mode = 0; mode < 4; ++mode)
    for (destruct = 0; destruct < 2; ++destruct) {
        xfuture *input = NULL, *result;
        xpromise* promise;
        const xerror* error;
        if (!smoke) xrtMemDebugSnapshot(&before);
        dropped = after = 0; raising = (int)destruct;
        promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
        result = match_async_start(kind, input);
        assert(result && !xrtFutureDone(result) && dropped == 0 && after == 0 && !xrtGetError());
        ++suspensions;
        if (mode == 1) assert(xrtPromiseReject(promise, inputError));
        else if (mode == 2) assert(xrtPromiseCancel(promise));
        else if (mode == 3) assert(xrtPromiseClose(promise));
        else assert(xrtPromiseResolve(promise, &value));
        xrtPromiseDestroy(promise);
        assert(xrtFutureWait(result) == 0);
        error = xrtFutureError(result);
        if (mode >= 2) {
            /* Automatic destructor errors cannot replace an input terminal
             * outcome. This differs from explicit source operations in finally. */
            assert(xrtFutureState(result) == (mode == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED) && !error);
        } else if (mode || destruct) {
            assert(xrtFutureState(result) == XFUTURE_FAILED && error);
            assert(xrtErrorCode(error) == (mode ? 88 : 77));
        } else {
            const int64_t* output = (const int64_t*)xrtFutureValue(result);
            assert(xrtFutureState(result) == XFUTURE_RESOLVED && !error && output);
            assert(*output == (kind == 0 || kind == 4 || kind == 5 || kind == 7 ? 20 : 5));
        }
        xrtFutureDestroy(result); xrtFutureDestroy(input);
        assert(dropped == 2 && after == (mode || destruct ? 0u : 1u) && !xrtGetError());
        if (!smoke) {
            xrtMemDebugSnapshot(&final);
            assert(before.LiveCount == final.LiveCount && before.LiveBytes == final.LiveBytes);
            assert(final.AllocCount - before.AllocCount == final.FreeCount - before.FreeCount);
            assert(before.InvalidFreeCount == final.InvalidFreeCount && before.DoubleFreeCount == final.DoubleFreeCount);
            assert(before.UseAfterFreeCount == final.UseAfterFreeCount);
        }
    }
    dropped = after = 0; raising = 0;
    match_async_keep();
    assert(dropped == 2 && after == 1 && !xrtGetError());
    match_async_host_fini(); assert(!xrtGetError());
    xrtErrorFree(cleanupError); xrtErrorFree(inputError);
    if (!smoke) {
        xrtMemDebugSnapshot(&final);
        assert(initial.LiveCount == final.LiveCount && initial.LiveBytes == final.LiveBytes);
        printf("async match cleanup: %u real suspensions, %llu balanced allocations\n", suspensions,
            (unsigned long long)(final.AllocCount - initial.AllocCount));
    } else puts("async match cleanup: generated C-host smoke passed");
    return 0;
}
