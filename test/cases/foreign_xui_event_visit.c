#include "foreign_xui_event_visit.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void fixtureEventEnableTracking(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
}

void fixtureEventBeginSample(void)
{
    xrtMemDebugSnapshot(&before);
}

void fixtureEventEndSample(int language)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    printf("Event visitor heap %s: alloc=%llu free=%llu realloc=%llu\n",
        language ? "language" : "native",
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

static int viewportVisitor(const xui_event_t* event, void* user)
{
    assert(event->iType == XUI_EVENT_VIEWPORT && event->fX == *(int*)user);
    return XUI_OK;
}

static void visitViewport(xui_context context, int width)
{
    bool visited = false;
    assert(xuiInputViewport(context, width, 100) == XUI_OK);
    assert(xuiPollEventVisit(context, viewportVisitor, &width, &visited) == XUI_OK && visited);
}

void fixtureEventNativeSample(xui_context context)
{
    visitViewport(context, 100);
    fixtureEventBeginSample();
    for (int n = 0; n < 128; ++n) visitViewport(context, 300 + n);
    fixtureEventEndSample(0);
}
