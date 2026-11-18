#include "foreign_xui_inventory_value.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void inventoryValueHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

static bool balanced(const xmemdebugsnapshot* after)
{
    return after->LiveCount == before.LiveCount && after->LiveBytes == before.LiveBytes &&
        after->InvalidFreeCount == before.InvalidFreeCount && after->DoubleFreeCount == before.DoubleFreeCount;
}

bool inventoryValueHeapBalanced(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    return balanced(&after);
}

void inventoryValueHeapReport(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(balanced(&after));
    printf("XUI_INVENTORY_VALUE_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

void inventoryValueBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}

bool inventoryValueFailureTriggered(void)
{
    return xrtMemDebugFailTriggered();
}

void inventoryValueNativeLoop(xui_widget grid, int count)
{
    for (int index = 0; index < count; ++index) {
        xvalue* snapshot = NULL;
        assert(xuiInventoryGridToXValue(grid, &snapshot) == XUI_OK && snapshot != NULL);
        xrtValueRelease(snapshot);
    }
}
