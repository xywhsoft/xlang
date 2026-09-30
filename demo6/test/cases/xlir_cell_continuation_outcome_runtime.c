#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void cell_continuation_prepare(void), cell_continuation_finish(void);
extern void cell_continuation_wait_idle(void);
extern void cell_continuation_close(int, bool);
extern unsigned cell_continuation_yield_fault(int, int);
extern xfuture* cell_continuation_launch(xfuture*, xfuture*, bool);
extern void cell_continuation_produce(xpromise*, int, int64_t*);
static int selectedPhase, selectedFault, smoke;
static unsigned reached, armed, everArmed;
static unsigned executions, faults, resumedFaults, allocationsInHook;
static int64_t payloads[2] = {10, 20};
static int dropId, dropMode;
static unsigned drops[3], dropSequence[2], dropCount, managedCases;
static xfuture* activeOutput;

int cell_continuation_dropped(int id)
{
    assert(id >= 1 && id <= 2 && !drops[id] && dropCount < 2);
    ++drops[id]; dropSequence[dropCount++] = (unsigned)id;
    if (activeOutput) assert(!xrtFutureDone(activeOutput));
    return id == dropId ? dropMode : 0;
}

void cell_continuation_mark(int64_t phase)
{
    assert(phase >= 0 && phase <= 1 && reached == (1u << (unsigned)phase) - 1u);
    reached |= 1u << (unsigned)phase;
    if (phase == selectedPhase) {
        assert(!armed && !everArmed); armed = everArmed = 1;
        // Arm inside the resumed language body, never before Promise completion.
        continuation_owner_fault_arm(selectedFault);
    }
}
xfuture* cell_continuation_input(void) { assert(0 && "reachability helper must not execute"); return NULL; }
void cell_continuation_fault_finish(int64_t phase)
{
    if (phase != selectedPhase) return;
    assert(armed);
    if (selectedFault >= 0 && !xrtMemDebugFailTriggered())
        fprintf(stderr, "cell continuation fault did not trigger: phase=%d fault=%d reached=%u\n",
            selectedPhase, selectedFault, reached);
    allocationsInHook = continuation_owner_fault_end(selectedFault); armed = 0;
}
static unsigned async_case(unsigned ready, int phase, int fault, int terminalPhase, int mode)
{
    xmemdebugsnapshot before; xfuture *inputs[2] = {NULL, NULL}, *result;
    xpromise* promises[2]; xfutureresult outcome; unsigned i; bool observed;
    if (!smoke) xrtMemDebugSnapshot(&before);
    selectedPhase = phase; selectedFault = fault; reached = armed = everArmed = allocationsInHook = 0;
    for (i = 0; i < 2; ++i) {
        promises[i] = xrtPromiseCreate(&inputs[i], NULL); assert(promises[i] && inputs[i]);
        if (ready & (1u << i)) cell_continuation_produce(promises[i], (int)i == terminalPhase ? mode : 0, &payloads[i]);
    }
    result = cell_continuation_launch(inputs[0], inputs[1], false);
    for (i = 0; i < 2 && !xrtFutureDone(result); ++i) {
        if (ready & (1u << i)) continue;
        cell_continuation_produce(promises[i], (int)i == terminalPhase ? mode : 0, &payloads[i]);
    }
    assert(xrtFutureWaitFor(result, 15000000) == XWAIT_OK);
    observed = xrtFutureResult(result, &outcome);
    if (!observed || xrtGetError()) {
        const xerror* error = xrtGetError();
        fprintf(stderr, "cell continuation observation failed: ready=%u phase=%d fault=%d terminal=%d mode=%d reached=%u done=%d observed=%d error=%s\n",
            ready, phase, fault, terminalPhase, mode, reached,
            xrtFutureDone(result) ? 1 : 0, observed ? 1 : 0,
            error != NULL ? xrtErrorMessage(error) : "none");
    }
    assert(observed && !xrtGetError());
    if (phase >= 0) assert(everArmed);
    if (fault >= 0) {
        assert(outcome.State == XFUTURE_FAILED && outcome.Error && !outcome.Value && xrtErrorKind(outcome.Error) == XERR_MEMORY);
        ++faults; if (!(ready & (1u << (unsigned)phase))) ++resumedFaults;
    } else if (mode) {
        assert(reached == (1u << (unsigned)terminalPhase) - 1u && !outcome.Value);
        assert(outcome.State == (mode == 1 ? XFUTURE_FAILED : mode == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED));
        if (mode == 1) assert(outcome.Error && xrtErrorCode(outcome.Error) == 71);
    } else assert(outcome.State == XFUTURE_RESOLVED && outcome.Value && !outcome.Error && *(int64_t*)outcome.Value == 9214 && reached == 3);
    xrtFutureDestroy(result);
    for (i = 0; i < 2; ++i) { xrtPromiseDestroy(promises[i]); xrtFutureDestroy(inputs[i]); }
    cell_continuation_wait_idle();
    assert(!xrtGetError()); if (!smoke) continuation_owner_fault_balanced(&before);
    ++executions; return allocationsInHook;
}
static void managed_case(unsigned ready, int terminalPhase, int mode, int raisingId, int raisingMode)
{
    xmemdebugsnapshot before; xfuture *inputs[2] = {NULL, NULL}, *result;
    xpromise* promises[2]; xfutureresult outcome; unsigned i;
    int sourceWins = mode && (terminalPhase == 0 || raisingId != 1);
    if (!smoke) xrtMemDebugSnapshot(&before);
    selectedPhase = -1; reached = 0; dropId = raisingId; dropMode = raisingMode;
    memset(drops, 0, sizeof(drops)); dropCount = 0;
    for (i = 0; i < 2; ++i) {
        promises[i] = xrtPromiseCreate(&inputs[i], NULL); assert(promises[i] && inputs[i]);
        if (ready & (1u << i)) cell_continuation_produce(promises[i], (int)i == terminalPhase ? mode : 0, &payloads[i]);
    }
    result = cell_continuation_launch(inputs[0], inputs[1], true);
    activeOutput = result;
    for (i = 0; i < 2 && !xrtFutureDone(result); ++i) {
        if (ready & (1u << i)) continue;
        cell_continuation_produce(promises[i], (int)i == terminalPhase ? mode : 0, &payloads[i]);
    }
    assert(xrtFutureWaitFor(result, 15000000) == XWAIT_OK);
    activeOutput = NULL;
    assert(xrtFutureResult(result, &outcome) && !xrtGetError());
    assert(drops[1] == 1 && dropSequence[0] == 1);
    assert(drops[2] == (unsigned)(!(mode && terminalPhase == 0) && raisingId != 1));
    if (drops[2]) assert(dropSequence[1] == 2);
    if (sourceWins) {
        assert(!outcome.Value && outcome.State == (mode == 1 ? XFUTURE_FAILED : mode == 2 ? XFUTURE_CANCELLED : XFUTURE_CLOSED));
        if (mode == 1) assert(outcome.Error && xrtErrorCode(outcome.Error) == 71);
    } else if (raisingId) {
        assert(outcome.State == XFUTURE_FAILED && outcome.Error && !outcome.Value);
        if (raisingMode == 1) assert(xrtErrorCode(outcome.Error) == 90 + raisingId);
        else assert(xrtErrorKind(outcome.Error) == XERR_INTERNAL);
    } else assert(outcome.State == XFUTURE_RESOLVED && outcome.Value && *(int64_t*)outcome.Value == 3 && !outcome.Error);
    xrtFutureDestroy(result);
    for (i = 0; i < 2; ++i) { xrtPromiseDestroy(promises[i]); xrtFutureDestroy(inputs[i]); }
    cell_continuation_wait_idle();
    assert(!xrtGetError()); if (!smoke) continuation_owner_fault_balanced(&before);
    ++managedCases;
}
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after; unsigned ready, round, fault, count, generatorFaults = 0;
    int stop, take, phase, mode, id, raising;
    smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    cell_continuation_prepare();
    for (round = 0; round < (smoke ? 1u : 30u); ++round) {
        for (stop = 0; stop <= 5; ++stop) for (take = 0; take < 2; ++take) {
            if (!smoke) xrtMemDebugSnapshot(&before);
            cell_continuation_close(stop, take != 0); ++executions;
            if (!smoke) continuation_owner_fault_balanced(&before);
        }
        for (ready = 0; ready < 4; ++ready) {
            async_case(ready, -1, -1, -1, 0);
            for (phase = 0; phase < 2; ++phase) for (mode = 1; mode <= 3; ++mode)
                async_case(ready, -1, -1, phase, mode);
            for (id = 0; id <= 2; ++id) for (raising = 1; raising <= (id ? 2 : 1); ++raising) {
                managed_case(ready, -1, 0, id, raising);
                for (phase = 0; phase < 2; ++phase) for (mode = 1; mode <= 3; ++mode)
                    managed_case(ready, phase, mode, id, raising);
            }
        }
    }
    if (!smoke) for (round = 0; round < 10; ++round) {
        for (stop = 0; stop <= 4; ++stop) {
            xrtMemDebugSnapshot(&before); count = cell_continuation_yield_fault(stop, -1);
            continuation_owner_fault_balanced(&before);
            for (fault = 0; fault < count; ++fault) {
                xrtMemDebugSnapshot(&before); cell_continuation_yield_fault(stop, (int)fault);
                continuation_owner_fault_balanced(&before); ++generatorFaults;
            }
        }
        for (ready = 0; ready < 4; ++ready) for (phase = 0; phase < 2; ++phase) {
            count = async_case(ready, phase, -1, -1, 0); assert(count);
            for (fault = 0; fault < count; ++fault) async_case(ready, phase, (int)fault, -1, 0);
        }
    }
    cell_continuation_finish();
    if (!smoke) {
        assert(generatorFaults && faults && resumedFaults); continuation_owner_fault_balanced(&initial);
        xrtMemDebugSnapshot(&after);
        printf("cell continuation: %u executions + %u managed-cell destructor cases, %u generator + %u async real allocation faults (%u after pending resume), %llu balanced allocations\n",
            executions, managedCases, generatorFaults, faults, resumedFaults, (unsigned long long)(after.AllocCount - initial.AllocCount));
    } else printf("cell continuation C-host smoke: %u executions + %u destructor cases; every close, ready/pending await, input failure/cancellation/closure\n", executions, managedCases);
    return 0;
}
