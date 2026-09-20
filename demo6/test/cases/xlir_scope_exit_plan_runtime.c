#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>

void xlir_scope_prepare(void);
void xlir_scope_run(void);
void xlir_scope_finish(void);

int main(void)
{
    xmemdebugsnapshot before, after;
    int cycle;
    assert(xrtMemDebugEnable(true));
    xlir_scope_prepare();
    xlir_scope_run();
    xlir_scope_finish();
    xrtMemDebugSnapshot(&before);
    xlir_scope_prepare();
    for (cycle = 0; cycle < 1000; ++cycle) xlir_scope_run();
    xlir_scope_finish();
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(after.DoubleFreeCount == before.DoubleFreeCount);
    assert(after.InvalidFreeCount == before.InvalidFreeCount);
    assert(after.UseAfterFreeCount == before.UseAfterFreeCount);
    assert(xrtGetError() == NULL);
    printf("scope exit plans: 1000 class/cell/product/generic scopes and %llu balanced allocations\n",
        (unsigned long long)(after.AllocCount - before.AllocCount));
    return 0;
}
