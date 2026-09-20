#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void nullable_join_case(int);
int main(int argc, char** argv)
{
    int iteration, choose;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    xmemdebugsnapshot before, after;
    assert(argc == 1 || smoke); assert(smoke || xrtMemDebugEnable(true));
    nullable_join_case(0); nullable_join_case(1);
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 200; ++iteration)
        for (choose = 0; choose < 2; ++choose) nullable_join_case(choose);
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("nullable joins: 400 cases, %llu balanced allocations\n", (unsigned long long)(after.AllocCount - before.AllocCount));
    } else puts("nullable join smoke: 400 cases passed");
    return 0;
}
