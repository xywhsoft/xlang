#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void close_catch_prepare(void), close_catch_finish(void);
extern void close_catch_case(int, int, int);
extern int close_catch_failures(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before;
    int form, mode, limit, runs = 0, smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    close_catch_prepare();
    for (form = 0; form < 8; ++form) for (mode = 0; mode < 4; ++mode)
    for (limit = 1; limit <= 5; ++limit) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        close_catch_case(form, mode, limit); ++runs;
        if (!smoke) continuation_owner_fault_balanced(&before);
    }
    close_catch_finish();
    if (!smoke) continuation_owner_fault_balanced(&initial);
    printf("catch dispatch close: %d executions, %d semantic failures; memory checks %s\n",
        runs, close_catch_failures(), smoke ? "disabled" : "balanced");
    return close_catch_failures() ? 1 : 0;
}
