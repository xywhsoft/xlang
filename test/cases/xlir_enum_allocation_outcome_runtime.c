#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int enum_outcome_init(void);
extern void enum_outcome_finish(void);
extern int64_t enum_outcome_invoke(int kind, int caught);
#include "xlir_owner_fault_runtime.h"

static unsigned run(int kind, int caught, int fault)
{
    xmemdebugsnapshot before; int64_t value; xerror* error; unsigned allocations;
    xrtMemDebugSnapshot(&before); continuation_owner_fault_arm(fault);
    value = enum_outcome_invoke(kind, caught); error = xrtTakeError();
    allocations = continuation_owner_fault_end(fault);
    if (fault < 0) assert(value == (kind == 4 ? 16 : 9) && !error);
    else if (caught) assert(value == 77 && !error);
    else assert(value == 0 && error && xrtErrorKind(error) == XERR_MEMORY);
    xrtErrorFree(error); continuation_owner_fault_balanced(&before);
    return allocations;
}

int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    int round, kind, caught; unsigned faults = 0, executions = 0; xmemdebugsnapshot before, after;
    if (smoke) {
        assert(enum_outcome_init());
        for (kind = 0; kind < 6; ++kind) for (caught = 0; caught < 2; ++caught) {
            assert(enum_outcome_invoke(kind, caught) == (kind == 4 ? 16 : 9));
            assert(!xrtGetError());
        }
        enum_outcome_finish(); puts("enum allocation outcome C-host smoke: 12 executions"); return 0;
    }
    assert(xrtMemDebugEnable(true) && enum_outcome_init()); xrtMemDebugSnapshot(&before);
    for (round = 0; round < 20; ++round) for (kind = 0; kind < 6; ++kind) for (caught = 0; caught < 2; ++caught) {
        unsigned count = run(kind, caught, -1), fault; ++executions;
        assert(count >= 5);
        for (fault = 0; fault < count; ++fault) { run(kind, caught, (int)fault); ++faults; }
    }
    xrtMemDebugSnapshot(&after); continuation_owner_fault_balanced(&before);
    printf("enum allocation outcome: %u baseline executions, %u real allocation failures, %llu balanced allocations; empty/scalar/mixed/string/borrowed/nested and catch\n",
        executions, faults, (unsigned long long)(after.AllocCount - before.AllocCount));
    enum_outcome_finish(); return 0;
}
