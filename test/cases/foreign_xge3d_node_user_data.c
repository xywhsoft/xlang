#include "test/cases/foreign_xge3d_node_user_data.h"
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot snapshot;

void fixtureNodeUserBudget(int allowance)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter(allowance));
}

void fixtureNodeUserMeasure(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&snapshot);
}

void fixtureNodeUserReport(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == snapshot.LiveCount && after.LiveBytes == snapshot.LiveBytes);
    assert(after.InvalidFreeCount == snapshot.InvalidFreeCount && after.DoubleFreeCount == snapshot.DoubleFreeCount);
    printf("NODE_USER_HEAP alloc=%llu free=%llu realloc=%llu\n",
        (unsigned long long)(after.AllocCount - snapshot.AllocCount),
        (unsigned long long)(after.FreeCount - snapshot.FreeCount),
        (unsigned long long)(after.ReallocCount - snapshot.ReallocCount));
}
