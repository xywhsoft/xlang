#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void close_extended_prepare(void), close_extended_finish(void);
extern void close_extended_case(int, int, int);
extern int close_extended_failures(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before;
    int form, mode, limit, runs = 0;
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    close_extended_prepare();
    for (form = 0; form < 3; ++form)
        for (mode = 0; mode < (form ? 3 : 1); ++mode)
            for (limit = 1; limit <= (form ? 5 : 1); ++limit) {
                if (!smoke) xrtMemDebugSnapshot(&before);
                close_extended_case(form, mode, limit); ++runs;
                if (!smoke) continuation_owner_fault_balanced(&before);
            }
    close_extended_finish();
    if (!smoke) continuation_owner_fault_balanced(&initial);
    printf("extended close: %d executions, %d semantic failures; memory checks %s\n",
        runs, close_extended_failures(), smoke ? "disabled" : "balanced");
    return close_extended_failures() ? 1 : 0;
}
