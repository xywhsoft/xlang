#include "test/cases/foreign_xui_chart_queries.h"
#include <assert.h>
#include <stdio.h>
#include "foreign_xui_render_backend.c"
void fixtureChartView(xui_widget chart,int manual,double a,double b,double c,double d)
{
    double w=0,x=0,y=0,z=0;int flag=xuiChartGetViewRange(chart,&w,&x,&y,&z);
    assert(flag>=0 && flag==manual && a==w && b==x && c==y && d==z);
}
void fixtureChartBrush(xui_widget chart,int active,double a,double b,double c,double d)
{
    double w=0,x=0,y=0,z=0;int flag=xuiChartGetBrushRange(chart,&w,&x,&y,&z);
    assert(flag>=0 && flag==active && a==w && b==x && c==y && d==z);
}
void fixtureChartAnimation(xui_widget chart,int active,int enabled,float duration,float progress)
{
    int on=0;float a=0,b=0;int flag=xuiChartGetAnimation(chart,&on,&a,&b);
    assert(flag>=0 && flag==active && on==enabled && a==duration && b==progress);
}
void fixtureChartArea(xui_widget chart,int series,int enabled,uint32_t color)
{
    uint32_t value=0;int flag=xuiChartGetSeriesAreaFill(chart,series,&value);
    assert(flag>=0 && flag==enabled && value==color);
}
void fixtureChartRadius(xui_widget chart,int series,int enabled,float minimum,float maximum)
{
    float a=0,b=0;int flag=xuiChartGetSeriesValueRadius(chart,series,&a,&b);
    assert(flag>=0 && flag==enabled && a==minimum && b==maximum);
}
void fixtureChartColor(xui_widget chart,int series,int enabled,uint32_t minimum,uint32_t maximum)
{
    uint32_t a=0,b=0;int flag=xuiChartGetSeriesValueColor(chart,series,&a,&b);
    assert(flag>=0 && flag==enabled && a==minimum && b==maximum);
}
static xmemdebugsnapshot before;
void fixtureChartHeapBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}
void fixtureChartHeapEnd(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount &&
        after.ReallocCount==before.ReallocCount && after.LiveCount==before.LiveCount &&
        after.LiveBytes==before.LiveBytes);
    printf("Chart query heap %s: calls=6000 alloc=0 free=0 realloc=0\n",native ? "native":"language");
}
void fixtureChartNativeHot(xui_widget chart,int line,int scatter)
{
    for(int i=0;i<1000;++i){
        double a=0,b=0,c=0,d=0;int enabled=0;float duration=0,progress=0,low=0,high=0;
        uint32_t color=0,minimum=0,maximum=0;
        assert(xuiChartGetViewRange(chart,&a,&b,&c,&d)==1 && a==0 && b==10 && c==-5 && d==5);
        assert(xuiChartGetBrushRange(chart,&a,&b,&c,&d)==1 && a==1 && b==3 && c==-2 && d==4);
        assert(xuiChartGetAnimation(chart,&enabled,&duration,&progress)==0 && enabled==0 && duration==2 && progress==1);
        assert(xuiChartGetSeriesAreaFill(chart,line,&color)==1 && color==0xff123456u);
        assert(xuiChartGetSeriesValueRadius(chart,scatter,&low,&high)==1 && low==2 && high==12);
        assert(xuiChartGetSeriesValueColor(chart,scatter,&minimum,&maximum)==1 && minimum==0xff000001u && maximum==0xff000099u);
    }
}
void fixtureChartNativeOracle(void)
{
    xui_proxy proxy=NULL;xui_context context=NULL;xui_widget chart=NULL;
    xui_context_desc_t desc;xuiContextDescDefault(&desc);desc.iSystemFontPolicy=XUI_SYSTEM_FONT_DISABLED;
    assert(fixtureRenderProxy(&proxy)==XUI_OK && xuiCreateOwnedEx(proxy,&desc,&context)==XUI_OK);
    assert(xuiChartCreate(context,&chart,NULL)==XUI_OK);
    int line=-1,scatter=-1;assert(xuiChartAddSeries(chart,XUI_CHART_SERIES_LINE,"line",&line)==XUI_OK);
    assert(xuiChartAddSeries(chart,XUI_CHART_SERIES_SCATTER,"scatter",&scatter)==XUI_OK);
    double a=0,b=0,c=0,d=0;assert(xuiChartGetViewRange(chart,&a,&b,&c,&d)==0);
    assert(xuiChartSetViewRange(chart,0,10,-5,5)==XUI_OK && xuiChartSetBrushRange(chart,1,3,-2,4)==XUI_OK);
    assert(xuiChartSetAnimation(chart,0,2)==XUI_OK);
    assert(xuiChartSetSeriesAreaFill(chart,line,1,0xff123456u)==XUI_OK);
    assert(xuiChartSetSeriesValueRadius(chart,scatter,2,12)==XUI_OK);
    assert(xuiChartSetSeriesValueColor(chart,scatter,0xff000001u,0xff000099u)==XUI_OK);
    fixtureChartHeapBegin();fixtureChartNativeHot(chart,line,scatter);fixtureChartHeapEnd(1);
    uint32_t color=0;float low=13,high=17;
    assert(xuiChartGetSeriesAreaFill(chart,-1,&color)==0 && !color);
    assert(xuiChartGetSeriesValueRadius(chart,line,&low,&high)==XUI_ERROR_UNSUPPORTED && low==13 && high==17);
    assert(xuiChartGetSeriesValueRadius(chart,-1,&low,&high)==XUI_ERROR_INVALID_ARGUMENT && low==13 && high==17);
    assert(xuiWidgetDestroyChecked(chart)==XUI_OK && xuiDestroyChecked(context)==XUI_OK);
    assert(xuiProxyReleaseChecked(proxy)==XUI_OK && fixtureRenderBalanced());
    puts("XUI_CHART_NATIVE_OK");
}
