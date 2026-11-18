#include "foreign_xui_combobox_items.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

static xmemdebugsnapshot before;

void comboItemsHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void comboItemsHeapEnd(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    printf("XUI_COMBOBOX_ITEM_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

void comboItemsSetBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}

bool comboItemsFailureTriggered(void)
{
    return xrtMemDebugFailTriggered();
}

void comboItemsNativeRead(xui_widget combo)
{
    for (int index = 0; index < 1000; ++index) {
        xui_combobox_item_t item = {0};
        assert(xuiComboBoxGetItemDescriptor(combo, 0, &item) == XUI_OK);
        assert(strcmp(item.sText, "Office") == 0 && item.iValue == 20 && item.iIcon == 7);
    }
}

void comboItemsNativeWrite(xui_widget combo)
{
    const xui_combobox_item_t items[] = {
        {"Office", 20, 1, 0, 7, NULL},
        {"", 30, 0, 0, 8, NULL},
        {NULL, 0, 0, 1, 0, NULL},
    };
    for (int index = 0; index < 1000; ++index) {
        assert(xuiComboBoxSetItemData(combo, items, 3) == XUI_OK);
    }
}
