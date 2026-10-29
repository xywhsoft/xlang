#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void managed_storage_case(int);
int main(int argc, char** argv)
{
    int i, choose, smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    xmemdebugsnapshot before, after;
    assert(argc == 1 || smoke); assert(smoke || xrtMemDebugEnable(true));
    managed_storage_case(0); managed_storage_case(1);
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (i = 0; i < 200; ++i) for (choose = 0; choose < 2; ++choose) managed_storage_case(choose);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
            fprintf(stderr, "managed storage leak: allocations=%llu frees=%llu liveDelta=%lld bytesDelta=%lld\n",
                (unsigned long long)(after.AllocCount - before.AllocCount),
                (unsigned long long)(after.FreeCount - before.FreeCount),
                (long long)after.LiveCount - (long long)before.LiveCount,
                (long long)after.LiveBytes - (long long)before.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("managed storage: 400 cases, %llu balanced allocations\n", (unsigned long long)(after.AllocCount - before.AllocCount));
    } else puts("managed storage smoke: 400 cases passed");
    return 0;
}
