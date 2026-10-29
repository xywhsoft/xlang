#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "test/cases/xlir_owner_fault_runtime.h"
extern void payload_fault_prepare(void), payload_fault_finish(void);
extern void payload_fault_ambient(void);
extern int payload_fault_run(int, int);
static int parse_int_arg(const char* text)
{
    char* end = NULL;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    assert(errno == 0 && end != text && *end == '\0' &&
        value >= INT_MIN && value <= INT_MAX);
    return (int)value;
}
static unsigned run(int caught, int fault)
{
    xmemdebugsnapshot before; unsigned count;
    xrtMemDebugSnapshot(&before); continuation_owner_fault_arm(fault);
    assert(payload_fault_run(caught, fault)); count = continuation_owner_fault_end(fault);
    continuation_owner_fault_balanced(&before); return count;
}
int main(int argc, char** argv)
{
    int round, caught; unsigned runs = 0, faults = 0; xmemdebugsnapshot initial;
    assert(xrtMemDebugEnable(true)); xrtMemDebugSnapshot(&initial); payload_fault_prepare();
    payload_fault_ambient();
    if (argc == 3) {
        unsigned allocations = run(parse_int_arg(argv[1]), parse_int_arg(argv[2]));
        payload_fault_finish(); continuation_owner_fault_balanced(&initial);
        printf("directed payload boxing: allocations=%u\n", allocations); return 0;
    }
    assert(argc == 1);
    for (round = 0; round < 20; ++round) for (caught = 0; caught < 2; ++caught) {
        unsigned count = run(caught, -1), fault; ++runs; assert(count > 0);
        for (fault = 0; fault < count; ++fault) { run(caught, (int)fault); ++runs; ++faults; }
    }
    payload_fault_finish(); continuation_owner_fault_balanced(&initial);
    printf("activation allocation: %u runs, %u real allocation failures, caught/propagated MemoryError and balanced memory\n", runs, faults);
    return 0;
}
