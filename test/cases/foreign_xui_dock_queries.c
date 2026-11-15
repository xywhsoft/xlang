#include "foreign_xui_dock_queries.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static xmemdebugsnapshot before;
void fixtureDockBudget(int n)
{if(n<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
void fixtureDockBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureDockReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Dock queries heap %s: alloc=%llu free=%llu realloc=%llu; record=%zu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount),sizeof(xui_dock_drop_info_t));
}
float fixtureDockInvalid(int kind){return kind==0?NAN:kind==1?INFINITY:-INFINITY;}
void fixtureDockCompare(xui_widget panel,int window,float x,float y,const xui_dock_drop_info_t* value)
{
    xui_dock_drop_info_t expected={0};assert(xuiDockPanelFindDropTarget(panel,window,x,y,&expected)==XUI_OK);
    assert(value->iSize==expected.iSize&&value->bValid==expected.bValid&&value->iWindow==expected.iWindow);
    assert(value->iPane==expected.iPane&&value->iRegion==expected.iRegion&&value->iSide==expected.iSide);
    assert(value->tRect.fX==expected.tRect.fX&&value->tRect.fY==expected.tRect.fY&&value->tRect.fW==expected.tRect.fW&&value->tRect.fH==expected.tRect.fH);
}
void fixtureDockNativeHot(xui_widget panel,int window,float x,float y)
{
    for(int i=0;i<1000;++i){
        xui_dock_drop_info_t value={0};assert(xuiDockPanelFindDropTarget(panel,window,x,y,&value)==XUI_OK&&value.bValid);
        assert(xuiDockPanelFindDropTarget(panel,window,-1,-1,&value)==XUI_OK&&!value.bValid&&value.iPane==-1);
    }
}
