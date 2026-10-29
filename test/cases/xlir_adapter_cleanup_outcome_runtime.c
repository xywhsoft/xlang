#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int adapter_cleanup_init(void);
extern void adapter_cleanup_finish(void);
extern int adapter_cleanup_invoke(int shape, int* retained);
#include "xlir_owner_fault_runtime.h"
static int live, opened[5], dropped[5], order, dropMode;
static xerror* errors[3];
void adapter_open_event(int id) { assert(id > 0 && id < 5); ++live; ++opened[id]; }
int adapter_drop_event(int id)
{
    assert(id >= 0 && id < 5);
    /* A failed constructor can finalize a zero-initialized object before its
     * user-level create hook ran; it has no opened test resource to close. */
    if (id == 0) return 0;
    --live; ++dropped[id]; order = order * 10 + id;
    return id <= 2 ? dropMode : 0;
}
xerror* adapter_test_error(int id) { assert(id >= 0 && id < 3); return xrtErrorRef(errors[id]); }
static unsigned run(int shape, int mode, int fault, int memory)
{
    xmemdebugsnapshot before; xerror* error; int ok, retained = 0, id; unsigned allocations = 0;
    assert(!live && !xrtGetError()); memset(opened, 0, sizeof(opened)); memset(dropped, 0, sizeof(dropped));
    order = 0; dropMode = mode;
    if (memory) { xrtMemDebugSnapshot(&before); continuation_owner_fault_arm(fault); }
    ok = adapter_cleanup_invoke(shape, &retained); error = xrtTakeError();
    if (memory) allocations = continuation_owner_fault_end(fault);
    if (ok != (fault < 0 && mode == 0 && shape != 4) || live != 0 ||
        (ok ? retained != (shape == 1 ? 1 : shape == 2 ? 2 : 0) : retained != 0)) {
        fprintf(stderr, "adapter cleanup shape=%d mode=%d fault=%d ok=%d live=%d retained=%d order=%d\n",
            shape, mode, fault, ok, live, retained, order); abort();
    }
    for (id = 1; id < 5; ++id) assert(opened[id] == dropped[id]);
    if (fault >= 0) assert(error && xrtErrorKind(error) == XERR_MEMORY);
    else if (shape == 4) assert(error == errors[0]);
    else if (mode == 1) assert(error == errors[2]);
    else assert(!error);
    if (fault < 0) {
        assert(opened[1] == 1 && opened[2] == 1);
        assert(order == (shape == 1 ? 213 : shape == 2 ? (mode == 0 ? 2134 : 2143) : 21));
    }
    xrtErrorFree(error);
    if (memory) continuation_owner_fault_balanced(&before);
    return allocations;
}
int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    int shape, mode, round; unsigned baseline = 0, faults = 0; xmemdebugsnapshot before, after;
    if (!smoke) assert(xrtMemDebugEnable(true));
    assert(adapter_cleanup_init());
    for (shape = 0; shape < 3; ++shape) {
        errors[shape] = xrtErrorCreate(XERR_ARGUMENT, "adapter-test", shape, "preallocated adapter failure");
        assert(errors[shape]);
    }
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (round = 0; round < (smoke ? 1 : 20); ++round) for (shape = 0; shape < 5; ++shape) {
        for (mode = 0; mode < 3; ++mode) { run(shape, mode, -1, !smoke); ++baseline; }
        if (!smoke) {
            unsigned fault, count = run(shape, 0, -1, 1);
            ++baseline;
            for (fault = 0; fault < count; ++fault) { run(shape, 0, (int)fault, 1); ++faults; }
        }
    }
    if (!smoke) { xrtMemDebugSnapshot(&after); continuation_owner_fault_balanced(&before); }
    printf("adapter cleanup: %u executions, %u real allocation failures, %llu balanced allocations; diagnostic/no-diagnostic destructor failures, first failure, direct scalar/owned/product/void outcomes\n",
        baseline, faults, smoke ? 0ull : (unsigned long long)(after.AllocCount - before.AllocCount));
    for (shape = 0; shape < 3; ++shape) { xrtErrorFree(errors[shape]); errors[shape] = NULL; }
    adapter_cleanup_finish(); assert(!live && !xrtGetError()); return 0;
}
