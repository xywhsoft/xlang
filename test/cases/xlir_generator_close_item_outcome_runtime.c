#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_owner_fault_runtime.h"
extern void close_item_prepare(void), close_item_finish(void);
extern void close_item_case(int, int, int);
extern int close_item_failures(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before;
    int iteration, consumer, item, cleanup, runs = 0;
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    close_item_prepare();
    for (iteration = 0; iteration < (smoke ? 1 : 100); ++iteration)
    for (consumer = 0; consumer < 3; ++consumer) for (item = 0; item < 3; ++item)
    for (cleanup = 0; cleanup < 3; ++cleanup) {
        if (!smoke) xrtMemDebugSnapshot(&before);
        close_item_case(consumer, item, cleanup); ++runs;
        if (!smoke) continuation_owner_fault_balanced(&before);
    }
    close_item_finish();
    if (!smoke) continuation_owner_fault_balanced(&initial);
    printf("generator owned item close: %d executions, %d semantic failures; memory checks %s\n",
        runs, close_item_failures(), smoke ? "disabled" : "balanced");
    return close_item_failures() ? 1 : 0;
}
