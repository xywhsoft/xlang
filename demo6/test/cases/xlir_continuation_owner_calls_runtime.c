#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern xfuture* owner_calls_alias(xfuture*, xfuture*, bool);
extern xfuture* owner_calls_pair(xfuture*, bool);
extern void owner_calls_generator(bool, int);
extern unsigned owner_calls_generator_fault(int);
extern void owner_calls_complete_checked(xpromise*, unsigned, int64_t*, xerror*);
static xerror *inputError, *callError;
static unsigned calls;
static int64_t inputValue = 10, secondValue = 20;
int64_t owner_calls_observe(const char* first, const char* second, bool fail)
{
    assert(first && second && !strcmp(first, "left") && !strcmp(second, "right")); ++calls;
    if (fail) xrtSetError(callError);
    return 42;
}
static void complete(xpromise* promise, unsigned mode, int64_t* payload)
{
    if (mode == 0) assert(xrtPromiseResolve(promise, payload));
    else if (mode == 1) assert(xrtPromiseReject(promise, inputError));
    else if (mode == 2) assert(xrtPromiseCancel(promise));
    else assert(xrtPromiseClose(promise));
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->DoubleFreeCount == after.DoubleFreeCount && before->InvalidFreeCount == after.InvalidFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
}
static unsigned setup_failures(void)
{
    unsigned round, stage, mode, next, choice, ready, fault, faults = 0;
    for (round = 0; round < 10; ++round)
    for (stage = 0; stage < 2; ++stage)
    for (mode = 0; mode < 4; ++mode)
    for (next = 0; next < 4; ++next)
    for (choice = 0; choice < 2; ++choice)
    for (ready = 0; ready < (stage ? 1u : 2u); ++ready)
    for (fault = 0; fault < 1; ++fault) {
        xfuture *input = NULL, *second = NULL, *result;
        xpromise *promise, *secondPromise;
        xmemdebugsnapshot before;
        const int64_t* value;
        int64_t expected = 5 + (mode ? 3 : 10) + (stage ? (next ? 7 : 20) : 7) + (stage ? 11 : (next ? 11 : 20));
        xrtMemDebugSnapshot(&before);
        promise = xrtPromiseCreate(&input, NULL); secondPromise = xrtPromiseCreate(&second, NULL);
        assert(promise && secondPromise && input && second);
        if (ready) complete(secondPromise, next, &secondValue);
        result = owner_calls_alias(input, second, choice != 0);
        assert(result && !xrtFutureDone(result) && !xrtGetError());
        if (stage) {
            complete(promise, mode, &inputValue);
            assert(!xrtFutureDone(result) && !xrtGetError());
        }
        /* Watch embeds its node and reuses the logical output Promise. Only
         * the next activation allocates; no hidden Promise/cancel/node exists
         * at a handoff. Allocation failure still enters the source catch CFG. */
        assert(xrtMemDebugFailAfter(fault));
        if (stage) complete(secondPromise, next, &secondValue);
        else complete(promise, mode, &inputValue);
        assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++faults;
        assert(!xrtGetError());
        if (!stage && !ready) {
            assert(!xrtFutureDone(result));
            complete(secondPromise, next, &secondValue);
        }
        assert(xrtFutureDone(result) && xrtFutureState(result) == XFUTURE_RESOLVED && !xrtFutureError(result));
        value = xrtFutureValue(result);
        if (!value || *value != expected)
            fprintf(stderr, "setup failure: stage=%u mode=%u next=%u ready=%u fault=%u actual=%lld expected=%lld\n",
                stage, mode, next, ready, fault, (long long)(value ? *value : -1), (long long)expected);
        assert(value && *value == expected);
        xrtPromiseDestroy(promise); xrtPromiseDestroy(secondPromise);
        xrtFutureDestroy(result); xrtFutureDestroy(input); xrtFutureDestroy(second);
        assert(!xrtGetError()); balanced(&before);
    }
    return faults;
}
static unsigned payload_failures(void)
{
    unsigned round, mode, next, choice, faults = 0;
    for (round = 0; round < 50; ++round)
    for (mode = 0; mode < 4; ++mode)
    for (next = 0; next < 4; ++next)
    for (choice = 0; choice < 2; ++choice) {
        xfuture *input = NULL, *second = NULL, *result;
        xpromise *promise, *secondPromise;
        xmemdebugsnapshot before; const xerror* error; xfutureresult observed;
        xrtMemDebugSnapshot(&before);
        promise = xrtPromiseCreate(&input, NULL); secondPromise = xrtPromiseCreate(&second, NULL);
        assert(promise && secondPromise && input && second);
        result = owner_calls_alias(input, second, choice != 0); assert(result && !xrtFutureDone(result));
        complete(promise, mode, &inputValue); assert(!xrtFutureDone(result));
        /* One successful next-frame allocation followed by the final result box.
         * The failure must not poison the producer's independent error scope. */
        assert(xrtMemDebugFailAfter(1));
        owner_calls_complete_checked(secondPromise, next, &secondValue, inputError);
        assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++faults;
        error = xrtFutureError(result);
        assert(xrtFutureState(result) == XFUTURE_FAILED && error && xrtErrorKind(error) == XERR_MEMORY);
        assert(xrtFutureResult(result, &observed) && observed.Value == NULL && !xrtGetError());
        xrtPromiseDestroy(promise); xrtPromiseDestroy(secondPromise);
        xrtFutureDestroy(result); xrtFutureDestroy(input); xrtFutureDestroy(second);
        assert(!xrtGetError()); balanced(&before);
    }
    return faults;
}
static unsigned registration_failures(void)
{
    unsigned round, mode, choice, ready, cases = 0;
    for (round = 0; round < 40; ++round) for (mode = 0; mode < 4; ++mode)
    for (choice = 0; choice < 2; ++choice) for (ready = 0; ready < 2; ++ready) {
        xmemdebugsnapshot before; xfuture *input = NULL, *result; xpromise* promise;
        const int64_t* value; xrtMemDebugSnapshot(&before);
        promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
        if (ready) owner_calls_complete_checked(promise, mode, &inputValue, inputError);
        /* A null Future from an external producer cannot register a Watch.
         * Both later awaits fail after physical handoff: this exercises real
         * registration rejection/rollback, not a fabricated allocator event. */
        result = owner_calls_alias(input, NULL, choice != 0); assert(result);
        if (!ready) owner_calls_complete_checked(promise, mode, &inputValue, inputError);
        assert(xrtFutureState(result) == XFUTURE_RESOLVED && !xrtFutureError(result));
        value = xrtFutureValue(result); assert(value && *value == 5 + (mode ? 3 : 10) + 7 + 11);
        xrtFutureDestroy(result); xrtPromiseDestroy(promise); xrtFutureDestroy(input);
        assert(!xrtGetError()); balanced(&before); ++cases;
    }
    return cases;
}
static unsigned acquisition_failures(void)
{
    unsigned round, kind, fault, faults = 0, generatorFaults = 0, caughtSetup = 0, failedStarts[2] = {0};
    for (round = 0; round < 20; ++round) {
        xmemdebugsnapshot before;
        unsigned allocations;
        xrtMemDebugSnapshot(&before); allocations = owner_calls_generator_fault(-1);
        balanced(&before); assert(allocations >= 3);
        for (fault = 0; fault < allocations; ++fault) {
            xrtMemDebugSnapshot(&before); owner_calls_generator_fault((int)fault);
            balanced(&before); ++generatorFaults;
        }
        for (kind = 0; kind < 2; ++kind) {
            unsigned startupAllocations = 0, attempt;
            for (attempt = 0; attempt <= startupAllocations; ++attempt) {
                xfuture *input = NULL, *result; xpromise* promise;
                int failIndex = attempt ? (int)attempt - 1 : -1;
                xrtMemDebugSnapshot(&before);
                promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
                continuation_owner_fault_arm(failIndex);
                result = kind ? owner_calls_pair(input, false) : owner_calls_alias(input, input, true);
                allocations = continuation_owner_fault_end(failIndex);
                if (failIndex < 0) {
                    startupAllocations = allocations; assert(startupAllocations && result && !xrtFutureDone(result));
                    complete(promise, 0, &inputValue);
                    assert(xrtFutureState(result) == XFUTURE_RESOLVED && !xrtFutureError(result) && !xrtGetError());
                } else {
                    const xerror* error = result ? xrtFutureError(result) : xrtGetError();
                    if (round == 0) printf("owner startup fault: kind=%u index=%d allocations=%u state=%d error=%d\n",
                        kind, failIndex, startupAllocations, result ? (int)xrtFutureState(result) : -1,
                        error ? (int)xrtErrorKind(error) : -1);
                    if (!error && result) {
                        const int64_t* value;
                        /* Later startup allocations install the first
                         * await activation. These fixtures explicitly catch
                         * that failure; it is not a root-acquisition failure. */
                        assert(!xrtGetError());
                        if (!xrtFutureDone(result)) complete(promise, 0, &inputValue);
                        value = xrtFutureValue(result);
                        assert(xrtFutureState(result) == XFUTURE_RESOLVED && value && *value == (kind ? 42 : 28) && !xrtGetError());
                        ++caughtSetup;
                    } else if (result) {
                        xfutureresult observed;
                        /* xrtFutureValue intentionally propagates a failed
                         * Future into TLS. Inspect the outcome without doing
                         * that while checking producer-scope isolation. */
                        assert(error && xrtErrorKind(error) == XERR_MEMORY && xrtFutureState(result) == XFUTURE_FAILED && !xrtGetError());
                        assert(xrtFutureResult(result, &observed) && observed.Value == NULL && !xrtGetError());
                        ++failedStarts[kind];
                    } else { assert(error && xrtErrorKind(error) == XERR_MEMORY); ++failedStarts[kind]; }
                    ++faults;
                }
                xrtFutureDestroy(result); xrtPromiseDestroy(promise); xrtFutureDestroy(input); xrtClearError();
                balanced(&before);
            }
        }
    }
    assert(failedStarts[0] && failedStarts[1] && caughtSetup);
    printf("continuation owner calls: %u generator and %u async startup allocation failures (%u uncaught, %u caught first-await setup); exact partial-owner rollback or typed recovery\n",
        generatorFaults, faults, failedStarts[0] + failedStarts[1], caughtSetup);
    return generatorFaults + faults;
}
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    unsigned round, kind, mode, next, choice, ready, stop, executions = 0, suspensions = 0;
    xmemdebugsnapshot initial, before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    inputError = xrtErrorCreate(XERR_STATE, "owner calls", 88, "input failure");
    callError = xrtErrorCreate(XERR_STATE, "owner calls", 93, "consuming call failure");
    assert(inputError && callError);
    for (round = 0; round < (smoke ? 1u : 100u); ++round) {
        for (choice = 0; choice < 2; ++choice)
        for (stop = 0; stop < (choice ? 2u : 3u); ++stop) {
            if (!smoke) xrtMemDebugSnapshot(&before);
            calls = 0; owner_calls_generator(choice != 0, (int)stop);
            assert(calls == (stop == 1 ? 0u : 1u) && !xrtGetError());
            if (!smoke) balanced(&before);
            ++executions;
        }
        for (kind = 0; kind < 2; ++kind)
        for (mode = 0; mode < 4; ++mode)
        for (next = 0; next < (kind ? 1u : 4u); ++next)
        for (choice = 0; choice < 2; ++choice)
        for (ready = 0; ready < 2; ++ready) {
            xfuture *input = NULL, *second = NULL, *result; xpromise *promise, *secondPromise = NULL; const xerror* failure;
            if (!smoke) xrtMemDebugSnapshot(&before);
            calls = 0; promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
            if (!kind) { secondPromise = xrtPromiseCreate(&second, NULL); assert(secondPromise && second); }
            if (ready) { complete(promise, mode, &inputValue); if (!kind) complete(secondPromise, next, &secondValue); }
            result = kind ? owner_calls_pair(input, choice != 0) : owner_calls_alias(input, second, choice != 0);
            assert(result && xrtFutureDone(result) == (ready != 0));
            if (!ready) {
                assert(!calls && !xrtGetError()); ++suspensions; complete(promise, mode, &inputValue);
                if (!kind) { assert(!xrtFutureDone(result)); ++suspensions; complete(secondPromise, next, &secondValue); }
            }
            xrtPromiseDestroy(promise);
            if (!kind) xrtPromiseDestroy(secondPromise);
            assert(xrtFutureWait(result) == 0); failure = xrtFutureError(result);
            if (kind && choice) {
                assert(xrtFutureState(result) == XFUTURE_FAILED && failure && xrtErrorCode(failure) == 93);
            } else {
                const int64_t* value = xrtFutureValue(result);
                assert(xrtFutureState(result) == XFUTURE_RESOLVED && !failure && value);
                if (*value != (kind ? 42 : 5 + (mode ? 3 : 10) + (next ? 7 : 20) + (next ? 11 : 20)))
                    fprintf(stderr, "owner calls value: kind=%u mode=%u next=%u choice=%u ready=%u actual=%lld expected=%d\n",
                        kind, mode, next, choice, ready, (long long)*value, kind ? 42 : 5 + (mode ? 3 : 10) + (next ? 7 : 20) + (next ? 11 : 20));
                assert(*value == (kind ? 42 : 5 + (mode ? 3 : 10) + (next ? 7 : 20) + (next ? 11 : 20)));
            }
            assert(calls == kind && !xrtGetError());
            xrtFutureDestroy(result); xrtFutureDestroy(input);
            if (!kind) xrtFutureDestroy(second);
            assert(!xrtGetError());
            if (!smoke) balanced(&before);
            ++executions;
        }
    }
    if (!smoke) printf("continuation owner calls: %u real setup allocation failures; local/nested rollback, typed catch, ready/pending and all input outcomes\n", setup_failures());
    if (!smoke) printf("continuation owner calls: %u real result-box allocation failures; FAILED MemoryError, balanced owners, isolated producer error scope\n", payload_failures());
    if (!smoke) printf("continuation owner calls: %u null-source cases with two rejected Watch registrations and resident rollbacks each\n", registration_failures());
    if (!smoke) assert(acquisition_failures());
    xrtErrorFree(inputError); xrtErrorFree(callError); assert(!xrtGetError());
    if (!smoke) {
        xrtMemDebugSnapshot(&after); balanced(&initial);
        printf("continuation owner calls: %u executions, %u genuine suspensions, %llu balanced allocations; async aliases, two-owner calls, ready/pending, all input outcomes, call failure and generator close\n",
            executions, suspensions, (unsigned long long)(after.AllocCount - initial.AllocCount));
    } else printf("continuation owner calls C-host smoke: %u executions\n", executions);
    return 0;
}
