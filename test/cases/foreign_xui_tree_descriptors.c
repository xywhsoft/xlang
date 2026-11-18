#include "foreign_xui_tree_descriptors.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void treeDescriptorsHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void treeDescriptorsHeapEnd(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    printf("XUI_TREE_DESCRIPTOR_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

void treeDescriptorsHeapBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}

void treeDescriptorsNativeHot(xui_widget tree)
{
    treeDescriptorsHeapBegin();
    for (int i = 0; i < 1000; ++i) {
        xui_tree_node_desc_t node;
        assert(xuiTreeViewGetNodeDescriptorById(tree, 11, &node) == XUI_OK);
        assert(node.tState.iDepth == 1 && node.sText[0] == 'C');
    }
    treeDescriptorsHeapEnd(1);
}
