#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void close_abrupt_prepare(void), close_abrupt_finish(void);
extern void close_abrupt_case(int, int, int);
extern int close_abrupt_failures(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before;
    int form, repeat, limit, runs = 0, smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    close_abrupt_prepare();
    for (form = 0; form < 3; ++form)
    for (repeat = 0; repeat < 2; ++repeat) for (limit = 1; limit <= 5; ++limit) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        close_abrupt_case(form, repeat, limit); ++runs;
        if (!smoke) continuation_owner_fault_balanced(&before);
    }
    close_abrupt_finish();
    if (!smoke) continuation_owner_fault_balanced(&initial);
    printf("abrupt finally close: %d executions, %d semantic failures; memory checks %s\n",
        runs, close_abrupt_failures(), smoke ? "disabled" : "balanced");
    return close_abrupt_failures() ? 1 : 0;
}
