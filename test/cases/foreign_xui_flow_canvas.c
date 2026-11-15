/* CPU test provider and independent owning-resource checks, never product glue. */
#include "foreign_xui_flow_canvas.h"
#include "foreign_xui_render_backend.c"
static xmemdebugsnapshot before;
void fixtureFlowCanvasBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureFlowCanvasEqual(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
    assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount);
}
void fixtureFlowCanvasBudget(int allowance)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}
void fixtureFlowCanvasReset(void){xrtMemDebugFailClear();xrtClearError();}
int fixtureFlowCanvasDefaults(xui_widget graph,xui_widget workflow)
{
    xui_flow_graph first=xuiFlowGraphWidgetGetGraph(graph);
    xui_workflow model=xuiWorkflowWidgetGetWorkflow(workflow);
    xui_widget child=xuiWorkflowWidgetGetCanvas(workflow);
    assert(first && model && child && first!=xuiWorkflowGetGraph(model));
    assert(xuiFlowGraphGetNodeCount(first)==0 && xuiWorkflowGetNodeCount(model)==0);
    assert(xuiFlowGraphWidgetGetGraph(child)==xuiWorkflowGetGraph(model));
    assert(xuiFlowGraphWidgetSetGraph(child,first,0)==XUI_ERROR_INVALID_STATE);
    assert(xuiFlowGraphWidgetGetGraph(child)==xuiWorkflowGetGraph(model));
    return XUI_OK;
}
int fixtureFlowCanvasContextMenu(xui_widget target)
{
    xui_context ui=xuiWidgetGetContext(target);assert(ui);
    int status=xuiInputViewport(ui,320,240);
    if(status==XUI_OK)status=xuiSetRootWidget(ui,target);
    if(status==XUI_OK)status=xuiWidgetSetRect(target,(xui_rect_t){0,0,320,240});
    if(status==XUI_OK)status=xuiLayout(ui);
    if(status==XUI_OK)status=xuiUpdate(ui,0.016f);
    if(status==XUI_OK)status=xuiInputPointerDown(ui,8,8,XUI_POINTER_BUTTON_RIGHT,XUI_POINTER_BUTTON_RIGHT);
    if(status==XUI_OK)status=xuiDispatchPendingEvents(ui);
    if(status==XUI_OK)status=xuiInputPointerUp(ui,8,8,XUI_POINTER_BUTTON_RIGHT,0);
    if(status==XUI_OK)status=xuiDispatchPendingEvents(ui);
    return status;
}
void fixtureFlowCanvasReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Flow hover heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),(unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureFlowCanvasNativeHot(xui_widget graph)
{
    int observed=0;
    for(int round=0;round<1000;round++){
        xui_flow_hit_t hit={0};assert(xuiFlowGraphWidgetGetHoverHit(graph,&hit)==XUI_OK);observed+=hit.iNode;
    }
    assert(observed==-1000);
}
