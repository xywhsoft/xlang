#include "foreign_xge_shape_references.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;
static double started;

void shapeReferencesHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    started = xrtTimer();
}

void shapeReferencesHeapEnd(int mode)
{
    double elapsedMs = (xrtTimer() - started) * 1000.0;
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    printf("XGE_SHAPE_REFERENCE_HEAP mode=%d alloc=%llu free=%llu realloc=%llu elapsedMs=%.6f\n", mode,
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount), elapsedMs);
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

void shapeReferencesNativeShare(xge_shape_ex shape, xge_shape_ex_scene scene)
{
    shapeReferencesHeapBegin();
    for (int index = 0; index < 1000; ++index) {
        xge_shape_ex sharedShape = NULL;
        xge_shape_ex_scene sharedScene = NULL;
        assert(xgeShapeExShareOwned(shape, &sharedShape) == XGE_OK);
        assert(xgeShapeExSceneShareOwned(scene, &sharedScene) == XGE_OK);
        xgeShapeExDestroy(sharedShape);
        xgeShapeExSceneDestroy(sharedScene);
    }
    shapeReferencesHeapEnd(3);
}
