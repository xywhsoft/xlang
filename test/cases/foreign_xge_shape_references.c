#include "foreign_xge_shape_references.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void shapeReferencesHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void shapeReferencesHeapEnd(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    printf("XGE_SHAPE_REFERENCE_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

void shapeReferencesHeapBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}

bool shapeReferencesAllocationFailed(void)
{
    return xrtMemDebugFailTriggered();
}

void shapeReferencesNativeHot(xge_shape_ex_scene scene)
{
    shapeReferencesHeapBegin();
    for (int index = 0; index < 1000; ++index) {
        xge_shape_ex shape = NULL;
        uint32_t id = 0;
        assert(xgeShapeExSceneGetAt(scene, 0, &shape) == XGE_OK);
        assert(xgeShapeExAddRef(shape) == XGE_OK);
        assert(xgeShapeExIdGet(shape, &id) == XGE_OK && id == 42);
        xgeShapeExDestroy(shape);
    }
    shapeReferencesHeapEnd(1);
}
