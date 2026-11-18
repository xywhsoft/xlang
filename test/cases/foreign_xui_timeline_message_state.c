#include "foreign_xui_timeline_message_state.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

static xmemdebugsnapshot before;

void stateHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void stateHeapEnd(int mode)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
    printf("XUI_TIMELINE_MESSAGE_HEAP mode=%d alloc=%llu free=%llu realloc=%llu\n", mode,
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount),
        (unsigned long long)(after.ReallocCount - before.ReallocCount));
}

void stateBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter((size_t)allowance));
}

bool stateAllocationFailed(void)
{
    return xrtMemDebugFailTriggered();
}

void stateNativeTimelineRead(xui_widget view, int layer, int span)
{
    for (int index = 0; index < 1000; ++index) {
        xui_timeline_layer_desc_t layerInfo;
        xui_timeline_frame_t frameInfo;
        xui_timeline_span_desc_t spanInfo;
        assert(xuiTimeLineViewGetLayerDescriptor(view, layer, &layerInfo) == XUI_OK);
        assert(xuiTimeLineViewGetFrame(view, layer, 4, &frameInfo) == XUI_OK);
        assert(xuiTimeLineViewGetSpanDescriptor(view, span, &spanInfo) == XUI_OK);
        assert(strcmp(layerInfo.sName, "Motion") == 0 && frameInfo.iType == XUI_TIMELINE_FRAME_KEY);
        assert(spanInfo.iId == span && strcmp(spanInfo.sLabel, "Pan") == 0);
    }
}

void stateNativeMessageRead(xui_widget view)
{
    for (int index = 0; index < 1000; ++index) {
        xui_message_node_t node;
        assert(xuiMessageListGetNodeDescriptor(view, 1, &node) == XUI_OK);
        assert(node.iType == XUI_MESSAGE_NODE_AUXILIARY && strcmp(node.sId, "review") == 0);
    }
}
