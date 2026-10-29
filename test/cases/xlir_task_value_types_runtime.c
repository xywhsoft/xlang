#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern int task_value_types_init(void);
extern void task_value_types_fini(void);
static bool report_live(const xmemdebugallocation* allocation, void* user)
{
    (void)user;
    fprintf(stderr, "task types live: %p %llu bytes at %s:%u\n", allocation->Address,
        (unsigned long long)allocation->Size, allocation->File ? allocation->File : "?", allocation->Line);
    return true;
}
int main(int argc, char** argv)
{
    unsigned round; int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    xmemdebugsnapshot before, after;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (round = 0; round < (smoke ? 1u : 30u); ++round) {
        int ok = task_value_types_init();
        if (!ok && xrtGetError()) fprintf(stderr, "task types: %s\n", xrtErrorMessage(xrtGetError()));
        assert(ok); task_value_types_fini(); assert(!xrtGetError());
    }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes) {
            fprintf(stderr, "task types balance: live %llu/%llu bytes %llu/%llu alloc %llu free %llu\n",
                (unsigned long long)before.LiveCount, (unsigned long long)after.LiveCount,
                (unsigned long long)before.LiveBytes, (unsigned long long)after.LiveBytes,
                (unsigned long long)(after.AllocCount - before.AllocCount),
                (unsigned long long)(after.FreeCount - before.FreeCount));
            xrtMemDebugVisitLive(report_live, NULL);
        }
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("task value types: %u module rounds, %llu balanced allocations\n", round,
            (unsigned long long)(after.AllocCount - before.AllocCount));
    } else puts("task value types generated C-host smoke passed");
    return 0;
}
