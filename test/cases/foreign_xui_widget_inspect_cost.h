#ifndef XL6_FIXTURE_XUI_WIDGET_INSPECT_COST_H
#define XL6_FIXTURE_XUI_WIDGET_INSPECT_COST_H
#include <xge3_runtime.h>

static xmemdebugsnapshot fixtureInspectBefore;

static int fixtureInspectHeapBegin(void)
{
    if(!xrtMemDebugEnabled() && !xrtMemDebugEnable(true))return 0;
    xrtMemDebugSnapshot(&fixtureInspectBefore);
    return 1;
}

static int fixtureInspectHeapUnchanged(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    return after.AllocCount==fixtureInspectBefore.AllocCount &&
           after.ReallocCount==fixtureInspectBefore.ReallocCount &&
           after.FreeCount==fixtureInspectBefore.FreeCount;
}

static int fixtureInspectNativeLoop(xui_context context)
{
    int checksum=0;
    for(int pass=0;pass<1000;++pass){
        xui_debug_widget_info_t info={0};
        if(xuiDebugWidgetInspectAtChecked(context,25,25,0,&info)!=XUI_OK ||
            !info.pWidget || !info.pParent)return -1;
        checksum+=info.iDepth+(int)info.tWorldRect.fW;
    }
    return checksum;
}
#endif
