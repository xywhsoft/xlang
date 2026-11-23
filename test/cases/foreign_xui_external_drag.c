#include "foreign_xui_external_drag.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void fixtureExternalDragEnableTracking(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
}

void fixtureExternalDragBeginSample(void)
{
    xrtMemDebugSnapshot(&before);
}

void fixtureExternalDragEndSample(int language)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    printf("External drag heap %s: alloc=%llu free=%llu realloc=%llu\n",
        language ? "language" : "native",
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

static void cycle(xui_context context, xui_data_object data)
{
    uint32_t effect = XUI_DRAG_EFFECT_MOVE;
    assert(xuiDragExternalEvent(context, XUI_DRAG_EXTERNAL_ENTER, 0, 0, 0,
        data, XUI_DRAG_EFFECT_COPY, XUI_DRAG_EFFECT_NONE, &effect) == XUI_OK);
    assert(effect == XUI_DRAG_EFFECT_NONE);
    assert(xuiDragExternalEvent(context, XUI_DRAG_EXTERNAL_OVER, 1, 1, 0,
        NULL, XUI_DRAG_EFFECT_NONE, XUI_DRAG_EFFECT_NONE, &effect) == XUI_OK);
    assert(effect == XUI_DRAG_EFFECT_NONE);
    assert(xuiDragExternalEvent(context, XUI_DRAG_EXTERNAL_CANCEL, 0, 0, 0,
        NULL, XUI_DRAG_EFFECT_NONE, XUI_DRAG_EFFECT_NONE, &effect) == XUI_OK);
    assert(effect == XUI_DRAG_EFFECT_NONE);
}

void fixtureExternalDragNativeSample(xui_context context, xui_data_object data)
{
    cycle(context, data);
    fixtureExternalDragBeginSample();
    for (int n = 0; n < 128; ++n) cycle(context, data);
    fixtureExternalDragEndSample(0);
}
