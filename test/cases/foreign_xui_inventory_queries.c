#include "foreign_xui_inventory_queries.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

void fixtureInventoryPrepare(xui_widget grid)
{
    xui_inventory_grid_layout_t layout={0};assert(xuiInventoryGridGetLayout(grid,&layout)==XUI_OK);
    layout.iColumns=4;layout.fSlotSize=20;layout.fSlotGap=4;layout.fPadding=0;layout.fIconPadding=1;layout.fBorderWidth=1;
    assert(xuiInventoryGridSetLayout(grid,&layout)==XUI_OK);
    /* Descriptor zero keeps the default padding; SetMetrics explicitly sets 0. */
    assert(xuiInventoryGridSetMetrics(grid,20,4,0,1,1)==XUI_OK&&xuiInventoryGridSetSlotCount(grid,40)==XUI_OK);
    xui_context ui=xuiWidgetGetContext(grid);xui_widget root=NULL;
    assert(xuiWidgetCreate(ui,&root)==XUI_OK&&xuiInputViewport(ui,200,120)==XUI_OK);
    assert(xuiWidgetSetRect(root,(xui_rect_t){0,0,200,120})==XUI_OK);
    assert(xuiWidgetSetPadding(grid,(xui_thickness_t){0})==XUI_OK);
    assert(xuiWidgetSetRect(grid,(xui_rect_t){20,30,80,50})==XUI_OK);
    assert(xuiWidgetAddChild(root,grid)==XUI_OK&&xuiSetRootWidget(ui,root)==XUI_OK&&xuiLayout(ui)==XUI_OK);
    xui_rect_t world=xuiWidgetGetWorldRect(grid);assert(world.fX==20&&world.fY==30&&world.fW==80&&world.fH==50);
}
float fixtureInventoryInvalid(int kind)
{return kind==0?NAN:(kind==1?INFINITY:-INFINITY);}
static xmemdebugsnapshot before;
void fixtureInventoryBudget(int n)
{if(n<0)xrtMemDebugFailClear();else {assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(n));}}
void fixtureInventoryBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureInventoryReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Inventory queries heap %s: alloc=%llu free=%llu realloc=%llu; hit=%zu range=%zu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount),sizeof(xui_inventory_hit_t),sizeof(xui_inventory_visible_range_t));
}
void fixtureInventoryNativeHot(xui_widget grid)
{
    for(int i=0;i<1000;++i){
        xui_inventory_hit_t hit={0};xui_inventory_visible_range_t range={0};
        assert(xuiInventoryGridGetVisibleRange(grid,&range)==XUI_OK);
        assert(range.iSize==sizeof(range)&&range.iFirstSlot==0&&range.iLastSlot==11&&range.iSlotCount==12&&range.iTotalRows==10&&range.iTotalColumns==4);
        assert(xuiInventoryGridHitTest(grid,20.5f,30.5f,&hit)==XUI_OK&&hit.iPart==XUI_INVENTORY_HIT_SLOT&&hit.iSlot==0);
        assert(hit.fX==0.5f&&hit.fY==0.5f&&hit.tRect.fX==0&&hit.tRect.fY==0);
        assert(xuiInventoryGridHitTest(grid,-1,-1,&hit)==XUI_OK&&hit.iPart==XUI_INVENTORY_HIT_NONE&&hit.iSlot==-1);
    }
}
