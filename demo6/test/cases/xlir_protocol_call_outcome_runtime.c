#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void protocol_outcome_cycle(void);
int main(int argc, char** argv)
{
    xmemdebugsnapshot initial, before, after;
    int i, smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    if (!smoke) xrtMemDebugSnapshot(&initial);
    protocol_outcome_cycle();
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (i = 0; i < 100; ++i) protocol_outcome_cycle();
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(initial.LiveCount == after.LiveCount && initial.LiveBytes == after.LiveBytes);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(initial.InvalidFreeCount == after.InvalidFreeCount && initial.DoubleFreeCount == after.DoubleFreeCount);
        assert(initial.UseAfterFreeCount == after.UseAfterFreeCount);
        printf("protocol outcome: %llu balanced allocations\n", (unsigned long long)(after.AllocCount - before.AllocCount));
    }
    puts("protocol outcome: 100 complete module lifecycles, joined witnesses, byval/byref/out, boxing, get/let, generator resume/failure/early close, 600 real async suspensions");
    return 0;
}
