/* Local CPU backend. Every object is explicitly closed before main returns,
 * so this fixture does not claim qualification of unloaded provider code. */
#include "test/cases/foreign_xui_icon_models.h"
#include "test/cases/foreign_xui_render_backend.c"
static bool verifyStyle;
static bool styleSeen;
static int observeStyle(xui_proxy proxy,xui_draw_context draw,const char* path,xui_rect_t viewBox,
    xui_rect_t target,const xui_path_style_t* style,float tolerance)
{
    if(verifyStyle){
        assert(style && style->iDashCount==2 && style->pDashPattern);
        assert(style->pDashPattern[0]==3.0f && style->pDashPattern[1]==2.0f);
        assert(style->iFillColor==0x10A030FF && style->iStrokeColor==0x4040FFFF);
        assert(style->fStrokeWidth==1.5f && style->fDashOffset==0.5f);
        styleSeen=true;
    }
    return __xuiTestDrawSvgPath(proxy,draw,path,viewBox,target,style,tolerance);
}
int fixtureIconProxy(xui_proxy* output)
{
    int status=fixtureRenderProxy(output);
    if(status==XUI_OK)backend.tProxy.drawSvgPath=observeStyle;
    return status;
}
void fixtureIconBudget(int allowance)
{
    if(allowance<0)xrtMemDebugFailClear();
    else {assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}
}
static xmemdebugsnapshot heapBefore;
void fixtureIconHeapBegin(void){xrtMemDebugSnapshot(&heapBefore);}
int fixtureIconHeapUnchanged(void)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    return now.AllocCount==heapBefore.AllocCount && now.FreeCount==heapBefore.FreeCount && now.ReallocCount==heapBefore.ReallocCount;
}
int fixtureIconLiveHeapUnchanged(void)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    return now.LiveCount==heapBefore.LiveCount && now.LiveBytes==heapBefore.LiveBytes &&
        now.InvalidFreeCount==heapBefore.InvalidFreeCount && now.DoubleFreeCount==heapBefore.DoubleFreeCount;
}
void fixtureIconDashes(void* output,size_t size)
{
    const float dashes[]={3.0f,2.0f};
    assert(output && size==sizeof(dashes));
    memcpy(output,dashes,sizeof(dashes));
}
void fixtureIconDrawStyled(xui_context context,xui_surface surface,xui_icon icon)
{
    xui_painter painter=NULL;
    assert(xuiPainterBegin(context,surface,&painter)==XUI_OK);
    verifyStyle=true;styleSeen=false;
    assert(xuiIconDraw(painter,icon,(xui_rect_t){0,0,16,16},NULL)==XUI_OK);
    verifyStyle=false;
    assert(styleSeen && xuiPainterEnd(painter)==XUI_OK);
}
void fixtureIconReferenceHot(xui_icon_reference_t* reference,xui_icon icon)
{
    for(int i=0;i<1000;++i){
        assert(xuiIconReferenceIsLive(reference));
        assert(xuiIconGetRefCount(icon)==2);
    }
}
