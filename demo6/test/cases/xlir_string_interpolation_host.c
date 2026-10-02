/* Generated interpolation module preincluded. Fail actual language allocations,
 * not an optional backing-cache refill that XRT can legally recover from. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char** argv)
{
    size_t point = argc == 2 ? (size_t)strtoull(argv[1], NULL, 10) : SIZE_MAX;
    xmemdebugsnapshot before, after;
    assert(argc == 1 || argc == 2);
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    if (point != SIZE_MAX) assert(xrtMemDebugFailAfter(point));
    int initialized = xlir_module_init();
    bool hit = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    if (hit) assert(!initialized && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    else assert(initialized && xrtGetError() == NULL);
    xlir_module_fini();
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    printf("interpolation logical allocation point=%zu hit=%d balanced\n", point, hit);
    return 0;
}
