#include "foreign_xui_timeline_queries.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int fixtureTimelinePrepare(xui_widget timeline)
{
    assert(xuiTimeLineViewSetMetrics(timeline,40,10,20,20)==XUI_OK);
    assert(xuiTimeLineViewSetFeatureFlags(timeline,0,0)==XUI_OK);
    assert(xuiTimeLineViewSetFrameCount(timeline,100)==XUI_OK);
    for(int i=0;i<10;++i){int layer=-1;assert(xuiTimeLineViewAddLayer(timeline,"Layer",&layer)==XUI_OK&&layer==i);}
    int span=-1;assert(xuiTimeLineViewAddSpan(timeline,1,5,8,0,"Span",&span)==XUI_OK);
    assert(xuiTimeLineViewSetCurrentFrame(timeline,6)==XUI_OK);
    assert(xuiTimeLineViewSelectFrame(timeline,1,9,1)==XUI_OK);
    xui_context ui=xuiWidgetGetContext(timeline);
    assert(xuiInputViewport(ui,160,100)==XUI_OK);
    assert(xuiWidgetSetRect(timeline,(xui_rect_t){20,30,160,100})==XUI_OK);
    assert(xuiSetRootWidget(ui,timeline)==XUI_OK&&xuiLayout(ui)==XUI_OK);
    assert(xuiTimeLineViewSetOffset(timeline,40,20)==XUI_OK);return span;
}

float fixtureTimelineInvalid(int kind)
{return kind==0?NAN:(kind==1?INFINITY:(kind==2?-INFINITY:XUI_LAYOUT_UNBOUNDED*2.0f));}

static xmemdebugsnapshot before;
void fixtureTimelineBudget(int n)
{if(n<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
void fixtureTimelineBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureTimelineReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Timeline queries heap %s: alloc=%llu free=%llu realloc=%llu; hit=%zu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount),sizeof(xui_timeline_hit_t));
}
void fixtureTimelineNativeHot(xui_widget timeline)
{
    for(int i=0;i<1000;++i){
        float x=0,y=0;xui_timeline_hit_t hit={0};
        assert(xuiTimeLineViewGetOffset(timeline,&x,&y)==XUI_OK&&x==40&&y==20);
        assert(xuiTimeLineViewHitTest(timeline,61,21,&hit)==XUI_OK);
        assert(hit.iType==XUI_TIMELINE_HIT_SPAN&&hit.iLayer==1&&hit.iFrame==6&&hit.tRect.fX==60&&hit.tRect.fY==20);
        assert(xuiTimeLineViewHitTest(timeline,-1,-1,&hit)==XUI_OK&&hit.iType==XUI_TIMELINE_HIT_NONE&&hit.iLayer==-1&&hit.iFrame==-1);
    }
}
