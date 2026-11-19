#include "foreign_xge_miniprogram_touch.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void touchHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void touchHeapEnd(int allocations)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount - before.AllocCount == (size_t)allocations);
    assert(after.FreeCount - before.FreeCount == (size_t)allocations);
    assert(after.ReallocCount == before.ReallocCount && after.LiveCount == before.LiveCount);
    printf("XGE_TOUCH_HEAP alloc=%d free=%d realloc=0\n", allocations, allocations);
}

void touchSetBudget(int allowance)
{
    if (allowance < 0) {
        assert(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
    } else {
        assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
        assert(xrtMemDebugFailAfter(allowance));
    }
}

void touchNativeHot(void)
{
    const xge_miniprogram_touch_t touches[] = {
        {UINT64_C(0x10000002A), 11.0f, 17.0f, 1.0f},
        {7, 23.0f, 29.0f, 1.0f},
    };
    touchHeapBegin();
    for (int i = 0; i < 1000; ++i) {
        xge_touch_point_t point;
        assert(xgeTouchFind(touches[0].iId, &point) == XGE_OK && point.fX == 22.0f);
    }
    touchHeapEnd(0);

    touchHeapBegin();
    for (int i = 0; i < 1000; ++i) {
        xge_input_event_info_t first, second;
        assert(xgeMiniProgramTouch(XGE_TOUCH_MOVE, touches, 2) == XGE_OK);
        assert(xgeInputEventRead(&first, NULL, 0) == XGE_INPUT_READ_READY);
        assert(xgeInputEventRead(&second, NULL, 0) == XGE_INPUT_READ_READY);
        assert(first.iPointerId == touches[0].iId && second.iPointerId == touches[1].iId);
    }
    touchHeapEnd(0);
}
