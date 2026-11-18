#include "foreign_xge_shape_traversal.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void shapeTraversalHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void shapeTraversalHeapEnd(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    printf("XGE_SHAPE_TRAVERSAL_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

void shapeTraversalHeapBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}

bool shapeTraversalAllocationFailed(void)
{
    return xrtMemDebugFailTriggered();
}

static int countShapes(const xge_shape_ex_scene_child_t* node, void* user)
{
    if (node->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE) ++*(int*)user;
    return 1;
}

void shapeTraversalNativeHot(xge_shape_ex_scene scene)
{
    int count = 0;
    shapeTraversalHeapBegin();
    for (int index = 0; index < 1000; ++index) {
        assert(xgeShapeExSceneTraverse(scene, countShapes, &count) == XGE_OK);
    }
    shapeTraversalHeapEnd(1);
    assert(count == 1000);
}
