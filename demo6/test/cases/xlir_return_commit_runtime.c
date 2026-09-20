#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>

void xlir_return_prepare(void);
void xlir_return_run(void);
void xlir_return_finish(void);

int main(void)
{
    xmemdebugsnapshot before, after;
    int cycle;
    assert(xrtMemDebugEnable(true));
    xlir_return_prepare(); xlir_return_run(); xlir_return_finish();
    xrtMemDebugSnapshot(&before);
    xlir_return_prepare();
    for (cycle = 0; cycle < 1000; ++cycle) xlir_return_run();
    xlir_return_finish();
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(after.DoubleFreeCount == before.DoubleFreeCount);
    assert(after.InvalidFreeCount == before.InvalidFreeCount);
    assert(after.UseAfterFreeCount == before.UseAfterFreeCount);
    assert(xrtGetError() == NULL);
    printf("return commit: 1000 parameter/local/product/finally transactions and %llu balanced allocations\n",
        (unsigned long long)(after.AllocCount - before.AllocCount));
    return 0;
}
