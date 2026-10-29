#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
#include "xlir_async_source_sync.h"
#include <stdatomic.h>
extern void owner_calls_prepare(void),owner_calls_finish(void);
extern xfuture* owner_calls_alias(xfuture*, xfuture*, bool);
extern xfuture* owner_calls_pair(xfuture*, bool);
extern void owner_calls_generator(bool, int);
extern unsigned owner_calls_generator_fault(int);
extern void owner_calls_complete_checked(xpromise*, unsigned, int64_t*, xerror*);
static xerror *inputError, *callError;
static unsigned calls;
static int64_t inputValue = 10, secondValue = 20;
static int faultPhase = -1, faultIndex = -1;
static atomic_uint phaseArms, phaseFaults;
void owner_calls_phase(int64_t phase)
{
    /* FailAfter is thread-local. Arm on the real worker, never the Promise
     * producer. A caught setup failure must not poison the next source phase. */
    if (phase == faultPhase + 1) {
        if (xrtMemDebugFailTriggered()) atomic_fetch_add(&phaseFaults, 1);
        xrtMemDebugFailClear();
    }
    if (phase == faultPhase) {
        atomic_fetch_add(&phaseArms, 1);
        assert(faultIndex >= 0 && xrtMemDebugFailAfter((uint64)faultIndex));
    }
}
static void arm_phase(int phase, int index)
{
    xlir_async_test_wait_retired();
    assert(!atomic_load(&phaseArms) && !atomic_load(&phaseFaults));
    faultPhase = phase; faultIndex = index;
}
static void take_phase_fault(void)
{
    bool tail;
    xlir_async_test_wait_retired(); tail = xlir_async_test_take_fault();
    assert(atomic_exchange(&phaseArms, 0) == 1);
    assert(atomic_exchange(&phaseFaults, 0) + (tail ? 1u : 0u) == 1);
    faultPhase = faultIndex = -1;
}
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
    unsigned round, stage, mode, next, choice, ready, faults = 0;
    for (round = 0; round < 10; ++round)
    for (stage = 0; stage < 3; ++stage)
    for (mode = 0; mode < 4; ++mode)
    for (next = 0; next < 4; ++next)
    for (choice = 0; choice < 2; ++choice)
    for (ready = 0; ready < 2; ++ready) {
        xfuture *input = NULL, *second = NULL, *result;
        xpromise *promise, *secondPromise;
        xmemdebugsnapshot before;
        const int64_t* value;
        int64_t expected = 5 + (stage == 0 ? 3 : mode ? 3 : 10) +
            (stage == 1 ? 7 : next ? 7 : 20) + (stage == 2 ? 11 : next ? 11 : 20);
        xrtMemDebugSnapshot(&before);
        promise = xrtPromiseCreate(&input, NULL); secondPromise = xrtPromiseCreate(&second, NULL);
        assert(promise && secondPromise && input && second);
        if (ready) complete(secondPromise, next, &secondValue);
        arm_phase(10 + 10 * (int)stage, 0);
        result = owner_calls_alias(input, second, choice != 0);
        xlir_async_test_wait_watches(1); assert(result && !xrtGetError());
        complete(promise, mode, &inputValue);
        if (!ready && stage) xlir_async_test_wait_watches(2);
        if (!ready) { complete(secondPromise, next, &secondValue); xlir_async_test_wait_retired(); }
        take_phase_fault(); ++faults;
        assert(xrtFutureDone(result) && xrtFutureState(result) == XFUTURE_RESOLVED && !xrtFutureError(result));
        value = xrtFutureValue(result);
        if (!value || *value != expected)
            fprintf(stderr, "setup failure: stage=%u mode=%u next=%u ready=%u fault=%u actual=%lld expected=%lld\n",
                stage, mode, next, ready, 0u, (long long)(value ? *value : -1), (long long)expected);
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
        arm_phase(40, 0);
        result = owner_calls_alias(input, second, choice != 0); assert(result && !xrtFutureDone(result));
        xlir_async_test_wait_watches(1);
        complete(promise, mode, &inputValue); xlir_async_test_wait_watches(2); assert(!xrtFutureDone(result));
        /* Final result allocation is armed on its real worker. Completion of
         * the producer is separately checked, with no inherited fault budget. */
        owner_calls_complete_checked(secondPromise, next, &secondValue, inputError);
        take_phase_fault(); ++faults;
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
        if (!ready) xlir_async_test_wait_watches(1);
        if (!ready) owner_calls_complete_checked(promise, mode, &inputValue, inputError);
        xlir_async_test_wait_retired();
        assert(xrtFutureState(result) == XFUTURE_RESOLVED && !xrtFutureError(result));
        value = xrtFutureValue(result); assert(value && *value == 5 + (mode ? 3 : 10) + 7 + 11);
        xrtFutureDestroy(result); xrtPromiseDestroy(promise); xrtFutureDestroy(input);
        assert(!xrtGetError()); balanced(&before); ++cases;
    }
    return cases;
}
static unsigned acquisition_failures(void)
{
    unsigned round, kind, fault, faults = 0, generatorFaults = 0, failedStarts[2] = {0};
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
            unsigned attempt;
            /* Discover every caller-thread launch prefix independently of
             * concurrent worker allocations. The first untriggered budget is
             * the complete normal path, not a fabricated setup failure. */
            for (attempt = 0; attempt < 32; ++attempt) {
                xfuture *input = NULL, *result; xpromise* promise;
                bool triggered;
                xrtMemDebugSnapshot(&before);
                promise = xrtPromiseCreate(&input, NULL); assert(promise && input);
                assert(xrtMemDebugFailAfter(attempt));
                result = kind ? owner_calls_pair(input, false) : owner_calls_alias(input, input, true);
                triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
                complete(promise, 0, &inputValue); xlir_async_test_wait_retired();
                /* Failed launch may drop its code lease synchronously on the
                 * caller. That observer consumes the thread-local trigger;
                 * combine both observations before classifying the prefix. */
                triggered = xlir_async_test_take_fault() || triggered;
                if (!triggered) {
                    const int64_t* value;
                    assert(result && !xrtGetError() && xrtFutureState(result) == XFUTURE_RESOLVED);
                    value = xrtFutureValue(result); assert(value && *value == (kind ? 42 : 35));
                } else {
                    const xerror* error = result ? xrtFutureError(result) : xrtGetError();
                    if (round == 0) printf("owner startup fault: kind=%u index=%u state=%d error=%d\n",
                        kind, attempt, result ? (int)xrtFutureState(result) : -1,
                        error ? (int)xrtErrorKind(error) : -1);
                    if (result) {
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
                if (!triggered) break;
            }
            assert(attempt > 0 && attempt < 32);
        }
    }
    assert(failedStarts[0] && failedStarts[1]);
    printf("continuation owner calls: %u generator and %u caller-thread async launch allocation failures; every acquisition prefix, exact partial-owner rollback; worker setup separately covered at all three await boundaries\n",
        generatorFaults, faults);
    return generatorFaults + faults;
}
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    unsigned round, kind, mode, next, choice, ready, stop, executions = 0, suspensions = 0;
    xmemdebugsnapshot initial, before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    owner_calls_prepare();
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
            if (ready) xlir_async_test_wait_retired();
            else xlir_async_test_wait_watches(1);
            assert(result && xrtFutureDone(result) == (ready != 0));
            if (!ready) {
                assert(!calls && !xrtGetError()); ++suspensions; complete(promise, mode, &inputValue);
                if (!kind) { xlir_async_test_wait_watches(2); assert(!xrtFutureDone(result)); ++suspensions; complete(secondPromise, next, &secondValue); }
                xlir_async_test_wait_retired();
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
    if (!smoke) {
        unsigned setup = setup_failures(), payload = payload_failures(), registrations = registration_failures();
        assert(setup == 1920 && payload == 1600 && registrations == 640 && acquisition_failures() > 0);
        printf("continuation owner calls: %u real setup allocation failures; local/nested rollback, typed catch, ready/pending and all input outcomes\n", setup);
        printf("continuation owner calls: %u real result-box allocation failures; FAILED MemoryError, balanced owners, isolated producer error scope\n", payload);
        printf("continuation owner calls: %u null-source cases with two rejected Watch registrations and resident rollbacks each\n", registrations);
        assert(executions == 8500 && suspensions == 7200);
    }
    owner_calls_finish();
    xrtErrorFree(inputError); xrtErrorFree(callError); assert(!xrtGetError());
    if (!smoke) {
        xrtMemDebugSnapshot(&after); balanced(&initial);
        printf("continuation owner calls: %u executions, %u genuine suspensions, %llu balanced allocations; async aliases, two-owner calls, ready/pending, all input outcomes, call failure and generator close\n",
            executions, suspensions, (unsigned long long)(after.AllocCount - initial.AllocCount));
    } else printf("continuation owner calls C-host smoke: %u executions\n", executions);
    return 0;
}
