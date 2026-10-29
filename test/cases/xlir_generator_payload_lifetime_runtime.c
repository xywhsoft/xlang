#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void payload_lifetime_prepare(void), payload_lifetime_finish(void);
extern void payload_lifetime_cycle(int, int, int);
extern int payload_lifetime_failures(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before;
    int round, kind, stop, mode, runs = 0;
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    payload_lifetime_prepare();
    for (round = 0; round < (smoke ? 1 : 100); ++round)
    for (kind = 0; kind < 2; ++kind) for (stop = -1; stop <= 2; ++stop) for (mode = 0; mode < 3; ++mode) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        payload_lifetime_cycle(kind, stop, mode); ++runs;
        if (!smoke) continuation_owner_fault_balanced(&before);
    }
    payload_lifetime_finish();
    if (!smoke) continuation_owner_fault_balanced(&initial);
    printf("generator payload lifetime: %d executions, %d semantic failures; memory checks %s\n",
        runs, payload_lifetime_failures(), smoke ? "disabled" : "balanced");
    return payload_lifetime_failures() ? 1 : 0;
}
