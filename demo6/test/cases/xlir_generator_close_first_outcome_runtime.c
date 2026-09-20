#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void close_outcome_prepare(void), close_outcome_finish(void);
extern void close_outcome_case(int, int);
extern void close_outcome_repeated(int, int, int);
extern int close_outcome_failures(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before;
    int iteration, consumer, cleanup, state, runs = 0, repeated = 0;
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    close_outcome_prepare();
    for (iteration = 0; iteration < (smoke ? 1 : 100); ++iteration)
    for (consumer = 0; consumer < 3; ++consumer) for (cleanup = 0; cleanup < 3; ++cleanup) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        close_outcome_case(consumer, cleanup); ++runs;
        if (!smoke) continuation_owner_fault_balanced(&before);
    }
    for (iteration = 0; iteration < (smoke ? 1 : 100); ++iteration)
    for (state = 0; state < 3; ++state) for (consumer = 0; consumer < 3; ++consumer)
    for (cleanup = 0; cleanup < 3; ++cleanup) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        close_outcome_repeated(state, consumer, cleanup); ++repeated;
        if (!smoke) continuation_owner_fault_balanced(&before);
    }
    close_outcome_finish();
    if (!smoke) continuation_owner_fault_balanced(&initial);
    printf("generator close first outcome: %d source executions, %d repeated-close sequences, %d semantic failures; memory checks %s\n",
        runs, repeated, close_outcome_failures(), smoke ? "disabled" : "balanced");
    return close_outcome_failures() ? 1 : 0;
}
