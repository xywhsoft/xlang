#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int cell_outcome_init(void);
extern void cell_outcome_finish(void);
extern int cell_outcome_run(int, int*, int*, long long*, unsigned*);
#include "xlir_owner_fault_runtime.h"
static unsigned creationFailures, invocationFailures, caughtFailures;
static unsigned run(int shape, int fault, int memory)
{
    const long long expected[] = {8, 6, 8, 1, 8, 8, 6, 8, 22, 1, 6, 14, 8};
    xmemdebugsnapshot before; xerror* error; int ok, nonnull, invoked, valid;
    long long value; unsigned finalies, allocations = 0;
    assert(!xrtGetError());
    if (memory) { xrtMemDebugSnapshot(&before); continuation_owner_fault_arm(fault); }
    ok = cell_outcome_run(shape, &nonnull, &invoked, &value, &finalies); error = xrtTakeError();
    if (memory) allocations = continuation_owner_fault_end(fault);
    if (fault < 0) valid = ok && nonnull == (shape != 12) && invoked == (shape == 12 ? -1 : 1) && value == expected[shape];
    else if (shape == 12) { valid = ok && !nonnull && invoked == -1 && value == -7; ++caughtFailures; }
    else if (!ok) { valid = !nonnull && invoked == -1 && value == 0; ++creationFailures; }
    else { valid = nonnull && invoked == 0 && value == 0; ++invocationFailures; }
    if (!valid || finalies != (unsigned)(shape == 12)) {
        fprintf(stderr, "cell allocation shape=%d fault=%d ok=%d nonnull=%d invoked=%d value=%lld finalies=%u\n",
            shape, fault, ok, nonnull, invoked, value, finalies); abort();
    }
    if (fault >= 0 && shape != 12) assert(error && xrtErrorKind(error) == XERR_MEMORY);
    else assert(!error);
    xrtErrorFree(error);
    if (memory) continuation_owner_fault_balanced(&before);
    return allocations;
}
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke"), shape, round;
    unsigned baseline = 0, faults = 0; xmemdebugsnapshot before, after;
    if (!smoke) assert(xrtMemDebugEnable(true));
    assert(cell_outcome_init());
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
    printf("cell allocation entries: 13 contexts, %u baseline runs, %u real allocation failures (%u creation, %u invocation, %u caught), %llu balanced allocations\n",
        baseline, faults, creationFailures, invocationFailures, caughtFailures,
        smoke ? 0ull : (unsigned long long)(after.AllocCount - before.AllocCount));
    cell_outcome_finish(); assert(!xrtGetError()); return 0;
}
