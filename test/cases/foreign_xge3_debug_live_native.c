#include "test/cases/foreign_xge3_debug_live.h"
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;
void fixtureDebugHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}
void fixtureDebugHeapEnd(int native)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("DEBUG_HEAP %s alloc=%llu free=%llu realloc=%llu\n",
           native ? "native" : "language",
           (unsigned long long)(after.AllocCount-before.AllocCount),
           (unsigned long long)(after.FreeCount-before.FreeCount),
           (unsigned long long)(after.ReallocCount-before.ReallocCount));
    assert(after.AllocCount==before.AllocCount);
    assert(after.FreeCount==before.FreeCount);
    assert(after.ReallocCount==before.ReallocCount);
}
void fixtureDebugNativeSamples(int repetitions)
{
    for (int i=0;i<repetitions;++i) {
        xge_debug_stats_t stats;
        assert(xgeDebugGetStats(&stats)==XGE_OK);
        assert(stats.iTextureCount>0 && stats.iTextureMemoryBytes>=16);
    }
}
void fixtureDebugBudget(int allowance)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    if (allowance<0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter(allowance));
}
