#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int loop_binding_init(void);
extern void loop_binding_finish(void);
extern int loop_binding_run(int, long long*);
#include "xlir_owner_fault_runtime.h"
static unsigned run(int shape, int fault, int memory)
{
    const long long expected[] = {4123, 4123, 4107, 4107, 707, 4107, 4107, 2234, 8, 207, 32};
    xmemdebugsnapshot before; xerror* error; int ok; long long value; unsigned count = 0;
    assert(!xrtGetError());
    if (memory) { xrtMemDebugSnapshot(&before); continuation_owner_fault_arm(fault); }
    ok = loop_binding_run(shape, &value); error = xrtTakeError();
    if (memory) count = continuation_owner_fault_end(fault);
    if (fault < 0 ? (!ok || value != expected[shape] || error != NULL)
                  : (ok || value != 0 || !error || xrtErrorKind(error) != XERR_MEMORY)) {
        fprintf(stderr, "loop binding shape=%d fault=%d ok=%d value=%lld error=%s\n",
            shape, fault, ok, value, error ? xrtErrorMessage(error) : "none"); abort();
    }
    xrtErrorFree(error);
    if (memory) continuation_owner_fault_balanced(&before);
    return count;
}
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke"), round, shape;
    unsigned baseline = 0, faults = 0; xmemdebugsnapshot before, after;
    if (!smoke) assert(xrtMemDebugEnable(true));
    assert(loop_binding_init());
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (round = 0; round < (smoke ? 1 : 20); ++round) for (shape = 0; shape < 11; ++shape) {
        unsigned fault, count = run(shape, -1, !smoke); ++baseline;
        if (!smoke) {
            if (shape != 7) assert(count > 0);
            for (fault = 0; fault < count; ++fault) { run(shape, (int)fault, 1); ++faults; }
        }
    }
    if (!smoke) { xrtMemDebugSnapshot(&after); continuation_owner_fault_balanced(&before); }
    printf("loop bindings: 11 contexts, %u baseline runs, %u real allocation failures, %llu balanced allocations\n",
        baseline, faults, smoke ? 0ull : (unsigned long long)(after.AllocCount - before.AllocCount));
    loop_binding_finish(); assert(!xrtGetError()); return 0;
}
