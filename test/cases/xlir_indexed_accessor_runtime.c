#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void indexed_accessor_cycle(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    int i, smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    indexed_accessor_cycle();
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (i = 0; i < 100; ++i) indexed_accessor_cycle();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(initial.InvalidFreeCount == after.InvalidFreeCount && initial.DoubleFreeCount == after.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("indexed accessor: %llu balanced allocations\n", (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    puts("indexed accessor: 100 complete module lifecycles, protocol/class indexed accessors, distinct generic instances, container-returning getters, failure and reverse owner cleanup");
    return 0;
}
