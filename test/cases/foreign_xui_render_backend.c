/* Deterministic CPU drawing, with REAL owned native Proxy/Font/Surface/Draw
 * controls and graph edges. No graphics, windows, network or private-layout
 * decoding. All provider callbacks are process-owned and resources explicitly
 * closed, so this does not qualify module unloading or real GPU rendering. */
#include "test/cases/foreign_xui_render_backend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tests/xui/xui_test_proxy.c"
enum {SURFACE=1,FONT,DRAW};
typedef struct Resource {void* managed;void* raw;unsigned kind;} Resource;
static Resource resources[256];
static xui_test_proxy_state_t backend;
static size_t references=1;
static char observed[64][256];static unsigned observedCount;
static void* unwrap(void* managed,unsigned kind)
{
    if(!managed)return NULL;
    for(unsigned i=0;i<256;++i)if(resources[i].managed==managed){assert(resources[i].kind==kind);return resources[i].raw;}
    assert(!"CPU backend received a foreign or expired resource");return NULL;
}
static int rawFree(xui_proxy proxy,void* raw,unsigned kind)
{
    if(kind==SURFACE){backend.tProxy.surfaceDestroy(proxy,(xui_surface)raw);return XUI_OK;}
    if(kind==FONT){backend.tProxy.fontDestroy(proxy,(xui_font)raw);return XUI_OK;}
    assert(kind==DRAW);return backend.tProxy.drawEnd(proxy,(xui_draw_context)raw);
}
static int finalize(xui_proxy proxy,void* payload)
{
    Resource* resource=*(Resource**)payload;assert(resource && resource->managed && resource->raw);
    Resource value=*resource;memset(resource,0,sizeof(*resource));return rawFree(proxy,value.raw,value.kind);
}
static int wrapResource(xui_proxy proxy,unsigned kind,void* raw,void** output,xui_surface target)
{
    assert(raw && output);*output=NULL;Resource* slot=NULL;
    for(unsigned i=0;i<256;++i)if(!resources[i].managed){slot=&resources[i];break;}
    assert(slot);void* payload=NULL;void* managed=NULL;int status;
    if(kind==SURFACE){xui_surface value=NULL;status=xuiSurfaceCreateOwned(proxy,sizeof(Resource*),finalize,&value,&payload);managed=value;}
    else if(kind==FONT){xui_font value=NULL;status=xuiFontCreateOwned(proxy,sizeof(Resource*),finalize,&value,&payload);managed=value;}
    else {xui_draw_context value=NULL;assert(kind==DRAW);status=xuiDrawCreateOwned(proxy,target,sizeof(Resource*),finalize,&value,&payload);managed=value;}
    if(status!=XUI_OK){xerror* first=xrtTakeError();(void)rawFree(proxy,raw,kind);xrtClearError();xrtSetErrorTake(first);return status;}
    *slot=(Resource){managed,raw,kind};*(Resource**)payload=slot;*output=managed;return XUI_OK;
}
static void observe(const xui_text_item_t* item)
{
    if(!item || !item->sText || observedCount>=64)return;
    size_t size=item->iTextSize<0 ? strlen(item->sText):(size_t)item->iTextSize;
    if(size>=256)size=255;
    memcpy(observed[observedCount],item->sText,size);observed[observedCount++][size]=0;
}
#include "test/cases/foreign_xui_render_adapters.inc"
static bool hold(const void* raw){assert(raw==&references);++references;return true;}
static bool drop(const void* raw){assert(raw==&references && references>1);--references;return true;}
static bool count(const void* raw,size_t* output){assert(raw==&references);*output=references;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user)
{assert(raw==&references);(void)visit;(void)user;return true;}
static const xrtownershipops operations={count,trace};
int fixtureRenderProxy(xui_proxy* output)
{
    assert(fixtureRenderBalanced());xuiTestProxyInit(&backend);fixtureRenderResetText();
    xui_proxy_t table;adapt(&table);
    const xui_callback_owner_t owner={{&references,&operations},hold,drop};
    return xuiProxyCreateOwned(&table,&owner,output);
}
int fixtureRenderFont(xui_proxy proxy,xui_font* output)
{return proxy->fontLoadMemory(proxy,output,"cpu",3,16,0);}
int fixtureRenderSurface(xui_proxy proxy,xui_surface* output)
{return proxy->surfaceCreateRGBA(proxy,output,640,480,NULL,0,XUI_SURFACE_USAGE_TARGET);}
int fixtureRenderBalanced(void)
{
    if(references!=1)return 0;
    for(unsigned i=0;i<256;++i)if(resources[i].managed)return 0;
    return 1;
}
int fixtureRenderSawText(const char* text)
{for(unsigned i=0;i<observedCount;++i)if(!strcmp(observed[i],text))return 1;return 0;}
void fixtureRenderResetText(void){observedCount=0;}
void fixtureRenderBudget(int allowance)
{
    if(allowance<0)xrtMemDebugFailClear();
    else {assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(allowance));}
}
static xstrview nativeText(xui_widget widget,xui_icon icon,void* user)
{
    (void)widget;(void)icon;(void)user;char* text=xrtMalloc(6);
    if(!text){xrtSetErrorKind(XERR_MEMORY);return (xstrview){0};}
    memcpy(text,"owned",6);return (xstrview){text,5};
}
int fixtureRenderNativeFormatter(xui_widget picker)
{
    const xui_callback_owner_t owner={{&references,&operations},hold,drop};
    return xuiIconPickerSetFormatterOwned(picker,nativeText,NULL,&owner);
}
static xmemdebugsnapshot heapBefore;
void fixtureRenderHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&heapBefore);}
void fixtureRenderHeapResult(int native)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    printf("Formatter frame heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(now.AllocCount-heapBefore.AllocCount),
        (unsigned long long)(now.FreeCount-heapBefore.FreeCount),
        (unsigned long long)(now.ReallocCount-heapBefore.ReallocCount));
}
