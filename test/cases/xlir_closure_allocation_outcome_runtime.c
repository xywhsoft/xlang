#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int closure_outcome_init(void);
extern void closure_outcome_finish(void);
extern int closure_outcome_run(int shape, int* nonnull, int* invoked, long long* value);
#include "xlir_owner_fault_runtime.h"
static unsigned creationFailures, invocationFailures, caughtFailures;
static unsigned run(int shape, int fault, int memory)
{
    xmemdebugsnapshot before; xerror* error; int ok, nonnull, invoked, valid; long long value; unsigned allocations = 0;
    assert(!xrtGetError());
    if (memory) { xrtMemDebugSnapshot(&before); continuation_owner_fault_arm(fault); }
    ok = closure_outcome_run(shape, &nonnull, &invoked, &value); error = xrtTakeError();
    if (memory) allocations = continuation_owner_fault_end(fault);
    if (fault < 0) valid = ok && nonnull == (shape != 9) && invoked == (shape == 9 ? -1 : 1) &&
        value == (shape == 3 ? 21 : 7);
    else if (shape == 9) { valid = ok && !nonnull && invoked == -1 && value == -7; ++caughtFailures; }
    else if (!ok) { valid = !nonnull && invoked == -1 && value == 0; ++creationFailures; }
    /* A restored field callable boxes the invocation result. Its allocation
     * failure belongs to invocation, not to the already completed factory.
     * Check both independent carriers without overwriting either outcome. */
    else { valid = nonnull && invoked == 0 && value == 0; ++invocationFailures; }
    if (!valid) {
        fprintf(stderr, "closure allocation shape=%d fault=%d ok=%d nonnull=%d invoked=%d value=%lld\n",
            shape, fault, ok, nonnull, invoked, value); abort();
    }
    if (fault >= 0 && shape != 9) assert(error && xrtErrorKind(error) == XERR_MEMORY);
    else assert(!error);
    xrtErrorFree(error);
    if (memory) continuation_owner_fault_balanced(&before);
    return allocations;
}
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    int shape, round; unsigned baseline = 0, faults = 0; xmemdebugsnapshot before, after;
    if (!smoke) assert(xrtMemDebugEnable(true));
    assert(closure_outcome_init());
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (round = 0; round < (smoke ? 1 : 20); ++round) for (shape = 0; shape < 13; ++shape) {
        unsigned fault, count = run(shape, -1, !smoke); ++baseline;
        if (!smoke) {
            assert(count > 0);
            for (fault = 0; fault < count; ++fault) { run(shape, (int)fault, 1); ++faults; }
        }
    }
    if (!smoke) {
        xrtMemDebugSnapshot(&after); continuation_owner_fault_balanced(&before);
        assert(creationFailures && invocationFailures && caughtFailures);
    }
    printf("closure allocation: 13 contexts, %u executions, %u real allocation failures (%u creation, %u invocation, %u caught), %llu balanced allocations; exact independent results and partial capture rollback\n",
        baseline, faults, creationFailures, invocationFailures, caughtFailures,
        smoke ? 0ull : (unsigned long long)(after.AllocCount - before.AllocCount));
    closure_outcome_finish(); assert(!xrtGetError()); return 0;
}
