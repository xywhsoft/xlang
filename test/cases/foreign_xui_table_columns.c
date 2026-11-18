#include "foreign_xui_table_columns.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void tableColumnsBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void tableColumnsEnd(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    printf("XUI_COLUMN_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

void tableColumnsBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}

void tableColumnsNativeHot(xui_widget table)
{
    tableColumnsBegin();
    for (int i = 0; i < 1000; ++i) {
        xui_table_column_desc_t desc;
        assert(xuiTableViewGetColumnDescriptor(table, 0, &desc) == XUI_OK);
        assert(desc.tSettings.iId == 21 && desc.sId && desc.sTitle);
    }
    tableColumnsEnd(2);
}
