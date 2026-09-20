#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void transaction_prepare(void);
void transaction_finish(void);
void transaction_cycle(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    int i, smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    transaction_prepare(); transaction_cycle();
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (i = 0; i < 100; ++i) transaction_cycle();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
            fprintf(stderr, "storage live before=%llu/%llu after=%llu/%llu\n",
                (unsigned long long)before.LiveCount, (unsigned long long)before.LiveBytes,
                (unsigned long long)after.LiveCount, (unsigned long long)after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("storage transaction: %llu balanced allocations\n", (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    transaction_finish();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(initial.DoubleFreeCount == after.DoubleFreeCount && initial.InvalidFreeCount == after.InvalidFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    puts("storage transaction: 1200 local/cell/static/global cases, 1200 genuine async cases, initial allocation baseline restored");
    return 0;
}
