/* CPU provider with actual native RC Font/Surface and owned shaping paint.
 * All resources close before this provider's module exits. No rendering stub
 * is used to claim graphics equivalence or unloaded-callback qualification. */
#include "foreign_xui_text_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tests/xui/xui_test_proxy.c"
static xui_test_proxy_state_t backend;
static size_t references=1;
static unsigned fonts,paints,shapes,surfaces,draws;
static bool ready=true,partialFailure;
static bool hold(const void* raw){assert(raw==&references);++references;return true;}
static bool drop(const void* raw){assert(raw==&references && references>1);--references;return true;}
static bool count(const void* raw,size_t* output){assert(raw==&references);*output=references;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user)
{assert(raw==&references);(void)visit;(void)user;return true;}
static const xrtownershipops operations={count,trace};
static int fontDrop(xui_proxy proxy,void* payload){(void)proxy;(void)payload;++fonts;return XUI_OK;}
static int surfaceDrop(xui_proxy proxy,void* payload){(void)proxy;(void)payload;++surfaces;return XUI_OK;}
static int metrics(xui_proxy proxy,xui_font font,xui_font_metrics_t* output)
{(void)proxy;assert(font);*output=(xui_font_metrics_t){.fSize=16,.fAscent=12,.fDescent=4,.fLineHeight=16};return XUI_OK;}
static int measure(xui_proxy proxy,const xui_text_item_t* item,xui_vec2_t* output)
{(void)proxy;int n=item->iTextSize<0?(int)strlen(item->sText):item->iTextSize;*output=(xui_vec2_t){n*8,16};return XUI_OK;}
typedef struct Paint {xui_font font;} Paint;
static bool paintReady(const void* raw){(void)raw;return ready;}
static bool paintTrace(const void* raw,xrtownershipvisitor visit,void* user)
{return visit(xuiFontOwnership(((const Paint*)raw)->font),user);}
static int paintDrop(void* raw)
{Paint* paint=raw;assert(xuiFontReleaseChecked(paint->font)==XUI_OK);xrtFree(paint);++paints;return XUI_OK;}
static const xui_text_paint_ownership_v1_t paintOps={sizeof(paintOps),XUI_TEXT_PAINT_OWNERSHIP_V1_TAG,paintReady,paintTrace,paintDrop};
static int shape(xui_proxy proxy,const xui_text_item_t* item,xui_text_shape_t* output)
{
    (void)proxy;int n=item->iTextSize<0?(int)strlen(item->sText):item->iTextSize;
    *output=(xui_text_shape_t){.iSize=sizeof(*output),.iTextSize=n,.iFlags=item->iFlags,
        .fAscent=12,.fDescent=4,.fLineHeight=16,.fHeight=16,.fWidth=n*8};
    if(n){output->pClusters=xrtCalloc(n,sizeof(*output->pClusters));if(!output->pClusters)return XUI_ERROR_OUT_OF_MEMORY;}
    output->iClusterCount=n;
    for(int i=0;i<n;++i)output->pClusters[i]=(xui_text_cluster_t){.iSize=sizeof(xui_text_cluster_t),.iTextStart=i,.iTextEnd=i+1,.fAdvance=8};
    Paint* paint=xrtMalloc(sizeof(*paint));if(!paint){xuiTextShapeFree(output);return XUI_ERROR_OUT_OF_MEMORY;}
    int status=xuiFontAddRef(item->pFont);if(status!=XUI_OK){xrtFree(paint);xuiTextShapeFree(output);return status;}
    paint->font=item->pFont;output->pPaint=paint;output->pPaintOwnership=&paintOps;++shapes;
    if(partialFailure){partialFailure=false;ready=false;assert(xrtMemDebugFailAfter(0));}
    return XUI_OK;
}
static int draw(xui_proxy proxy,xui_surface target,const xui_text_item_t* item,xui_rect_t rectangle,uint32_t color,uint32_t flags)
{(void)proxy;(void)rectangle;(void)color;(void)flags;assert(target && item->sText && item->pFont);++draws;return XUI_OK;}
int fixtureLayoutProxy(xui_proxy* output)
{
    xuiTestProxyInit(&backend);backend.tProxy.fontGetMetrics=metrics;backend.tProxy.textMeasure=measure;
    backend.tProxy.textShape=shape;backend.tProxy.textDraw=draw;
    const xui_callback_owner_t owner={{&references,&operations},hold,drop};
    return xuiProxyCreateOwned(&backend.tProxy,&owner,output);
}
int fixtureLayoutFont(xui_proxy proxy,xui_font* output)
{void* payload=NULL;return xuiFontCreateOwned(proxy,0,fontDrop,output,&payload);}
int fixtureLayoutSurface(xui_proxy proxy,xui_surface* output)
{void* payload=NULL;return xuiSurfaceCreateOwned(proxy,0,surfaceDrop,output,&payload);}
void fixtureLayoutReady(int value)
{if(value && !ready)assert(paints<shapes && !fonts);ready=value!=0;}
void fixtureLayoutBudget(int allowance)
{if(allowance<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}}
void fixtureLayoutArmPartialFailure(void)
{assert(ready && !partialFailure);partialFailure=true;assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));}
void fixtureLayoutCheck(void)
{assert(references==1 && fonts==1 && paints==shapes && surfaces==1 && draws==1);}
static xmemdebugsnapshot before;
void fixtureLayoutHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureLayoutHeapReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("Layout heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureLayoutNativeHot(xui_text_layout layout)
{
    for(int i=0;i<1000;++i){xui_vec2_t size={0};int lines=0,truncated=-1;
        assert(xuiTextLayoutGetState(layout,&size,&lines,&truncated)==XUI_OK);
        assert(size.fX==24 && size.fY==16 && lines==1 && truncated==0);}
}
void fixtureLayoutContextScopes(xui_text_layout layout)
{
    xui_text_layout_context_scope_t first={0},second={0};
    assert(xuiTextLayoutContextScopeBegin(layout,&first)==XUI_OK);
    assert(first.tContext.pContext==NULL);
    xui_context context=xuiTextLayoutGetScopedContext(layout);
    assert(context && context==xuiTextLayoutGetContext(layout));
    assert(xuiTextLayoutContextScopeBegin(layout,&second)==XUI_OK);
    assert(first.tContext.pContext==context && second.tContext.pContext==NULL);
    assert(xuiTextLayoutGetScopedContext(layout)==context && second.tContext.pContext==context);
    assert(xuiTextLayoutContextScopeEnd(&first)==XUI_OK);
    assert(xuiTextLayoutGetScopedContext(layout)==context);
    assert(xuiTextLayoutContextScopeEnd(&second)==XUI_OK);
}
