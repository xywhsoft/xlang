#include "src/runtime/xl6_stack.h"
#include <assert.h>
#include <stdio.h>
void clearProbeNativeV1(void);
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    clearProbeNativeV1(); xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
    puts("sequence clear: 96 error/fault/pending vectors, 3 last-owner cases, balanced");
    return 0;
}
