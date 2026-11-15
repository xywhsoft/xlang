#include "foreign_xui_table_geometry.h"
#include <assert.h>
#include <stdio.h>
void fixtureTableGeometryPrepare(xui_widget table)
{
    xui_table_view_column_t columns[2]={{0}};
    columns[0].sTitle="A";columns[0].fWidth=100;columns[1].sTitle="B";columns[1].fWidth=100;
    assert(xuiTableViewSetDefaultMetrics(table,100,20,20)==XUI_OK);
    assert(xuiTableViewSetColumnsCopy(table,columns,2)==XUI_OK);
    assert(xuiTableViewSetRows(table,NULL,50)==XUI_OK);
    xui_context ui=xuiWidgetGetContext(table);
    assert(xuiInputViewport(ui,120,80)==XUI_OK);
    xuiWidgetSetRect(table,(xui_rect_t){0,0,120,80});assert(xuiSetRootWidget(ui,table)==XUI_OK);
    assert(xuiLayout(ui)==XUI_OK&&xuiTableViewSetOffset(table,40,60)==XUI_OK);
}
static xmemdebugsnapshot before;
void fixtureTableGeometryBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureTableGeometryReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Table geometry heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureTableGeometryNativeHot(xui_widget table)
{
    for(int i=0;i<1000;++i){
        float x=0,y=0;xui_rect_t content={0},visible={0};int row=0,column=0;
        assert(xuiTableViewGetOffset(table,&x,&y)==XUI_OK);
        assert(xuiTableViewGetCellContentRect(table,3,1,&content)==XUI_OK);
        assert(xuiTableViewGetCellRect(table,3,1,&visible)==XUI_OK);
        assert(xuiTableViewGetItemAt(table,-1,-1,&row,&column)==XUI_OK);
        assert(x==40&&y==60&&content.fX==100&&content.fY==60&&row==-1&&column==-1);
    }
}
void fixtureTableGeometryBudget(int n)
{if(n<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
