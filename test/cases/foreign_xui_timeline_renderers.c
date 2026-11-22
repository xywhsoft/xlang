#include "test/cases/foreign_xui_timeline_renderers.h"
#include "test/cases/foreign_xui_render_backend.c"

static xmemdebugsnapshot timelineHeap;

void fixtureTimelineHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&timelineHeap);
}

int fixtureTimelineHeapUnchanged(void)
{
    xmemdebugsnapshot now;
    xrtMemDebugSnapshot(&now);
    return now.AllocCount == timelineHeap.AllocCount && now.FreeCount == timelineHeap.FreeCount &&
        now.ReallocCount == timelineHeap.ReallocCount;
}

void fixtureTimelineReadFrame(const xui_timeline_frame_t* frame, int layer)
{
    const volatile xui_timeline_frame_t* record = frame;
    for (int i = 0; i < 1000; ++i) {
        assert(record->iLayer == layer);
        assert(record->iFrame == 4);
        assert(record->iType == XUI_TIMELINE_FRAME_KEY);
    }
}
