#include "foreign_xui_geometry.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureGeometryHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}
void fixtureGeometryHeapReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Geometry heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureGeometryNativeHot(xui_widget widget)
{
    for(int i=0;i<1000;++i){
        float track=0,knob=0,radius=0;
        assert(xuiSliderGetMetrics(widget,&track,&knob,&radius)==XUI_OK);
        assert(track==6 && knob==18 && radius==3);
    }
}
