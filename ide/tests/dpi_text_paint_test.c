#include "app_xrt.h"
#include "platform/dpi_text_paint.h"
#include "platform/dpi_proxy.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"dpi_text_paint_test:%d: %s\n",__LINE__,#v);return 1; } } while(0)
static int failAllocation;
void* __real_calloc(size_t count,size_t size);
void* __wrap_calloc(size_t count,size_t size)
{ if(failAllocation) { failAllocation=0;return NULL; }return __real_calloc(count,size); }
typedef struct counts { int drops,traces,ready,status;uint64_t thread; } counts;
typedef struct payload { counts* counters; } payload;
static bool ready(const void* raw)
{ const counts* c=((const payload*)raw)->counters;return c->ready && c->thread==xrtThreadCurrentId(); }
static bool trace(const void* raw,xrtownershipvisitor visitor,void* user)
{
    counts* c=((const payload*)raw)->counters;++c->traces;
    return visitor((xrtownershipref){0},user);
}
static int drop(void* raw)
{
    payload* p=raw;counts* c=p->counters;++c->drops;free(p);
    if(c->status!=XUI_OK)xrtSetErrorKind(XERR_MEMORY);
    return c->status;
}
static const xui_text_paint_ownership_v1_t ownership={
    sizeof(ownership),XUI_TEXT_PAINT_OWNERSHIP_V1_TAG,ready,trace,drop
};
static xui_text_shape_t shape(counts* c)
{
    xui_text_shape_t result={0};payload* p=malloc(sizeof(*p));if(!p)return result;
    p->counters=c;c->thread=xrtThreadCurrentId();c->ready=1;
    result.iSize=sizeof(result);result.pPaint=p;result.pPaintOwnership=&ownership;
    result.iClusterCount=1;result.pClusters=xrtMalloc(sizeof(*result.pClusters));
    result.iCaretCount=1;result.pCarets=xrtMalloc(sizeof(*result.pCarets));return result;
}
static bool visitor(xrtownershipref ref,void* user) { (void)ref;++*(int*)user;return true; }
typedef struct attempt { xui_text_shape_t* shape;int status; } attempt;
static DWORD WINAPI foreignThread(void* data)
{ attempt* a=data;a->status=xuiTextShapeFreeChecked(a->shape);return 0; }
int main(int argc,char** argv)
{
    counts a={0},b={0};xui_text_shape_t logical=shape(&a),physical=shape(&b),unwrapped={0};
    CHECK(logical.pClusters && physical.pCarets);void* original=logical.pPaint;void* pixels=physical.pPaint;
    CHECK(xideDpiTextPaintWrap(&logical,&physical,2)==XUI_OK);CHECK(!physical.pPaint && !physical.pClusters);
    xideDpiTextPaintLogical(&logical,&unwrapped);CHECK(unwrapped.pPaint==original && unwrapped.pPaintOwnership==&ownership);
    CHECK(unwrapped.pClusters==logical.pClusters);
    CHECK(xideDpiTextPaintPhysical(&logical,2,&unwrapped)==XUI_OK && unwrapped.pPaint==pixels);
    CHECK(xideDpiTextPaintPhysical(&logical,1.25f,&unwrapped)==XUI_ERROR_UNSUPPORTED);
    int visits=0;CHECK(logical.pPaintOwnership->TraceRaw(logical.pPaint,visitor,&visits));
    CHECK(visits==2 && a.traces==1 && b.traces==1);
    b.ready=0;CHECK(xuiTextShapeFreeChecked(&logical)==XUI_STATUS_DEFERRED);
    CHECK(logical.pPaint && a.drops==0 && b.drops==0);b.ready=1;
    attempt at={&logical,0};HANDLE thread=CreateThread(NULL,0,foreignThread,&at,0,NULL);CHECK(thread);
    CHECK(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);
    CHECK(at.status==XUI_STATUS_DEFERRED && logical.pPaint && !a.drops && !b.drops);
    CHECK(xuiTextShapeFreeChecked(&logical)==XUI_OK);CHECK(!logical.pPaint && !logical.pClusters && !logical.pCarets);
    CHECK(a.drops==1 && b.drops==1);CHECK(xuiTextShapeFreeChecked(&logical)==XUI_OK);
    CHECK(!xrtGetError());
    for(int prior=0;prior<2;++prior) {
        counts c={0},d={0};c.status=XUI_ERROR_BACKEND_FAILED;d.status=XUI_ERROR_INVALID_STATE;
        logical=shape(&c);physical=shape(&d);CHECK(xideDpiTextPaintWrap(&logical,&physical,1.5f)==XUI_OK);
        const xerror* previous=NULL;if(prior) { xrtSetErrorKind(XERR_IO);previous=xrtGetError(); }
        CHECK(xuiTextShapeFreeChecked(&logical)==XUI_ERROR_BACKEND_FAILED);
        CHECK(!logical.pPaint && c.drops==1 && d.drops==1 && xrtGetError());
        if(previous)CHECK(xrtGetError()==previous);
        xrtErrorFree(xrtTakeError());
    }
    counts c={0},d={0};logical=shape(&c);physical=shape(&d);original=logical.pPaint;pixels=physical.pPaint;
    failAllocation=1;CHECK(xideDpiTextPaintWrap(&logical,&physical,2)==XUI_ERROR_OUT_OF_MEMORY);
    CHECK(logical.pPaint==original && physical.pPaint==pixels && !c.drops && !d.drops && !xrtGetError());
    CHECK(xuiTextShapeFreeChecked(&logical)==XUI_OK && xuiTextShapeFreeChecked(&physical)==XUI_OK);
    CHECK(c.drops==1 && d.drops==1);
    if(argc==2) {
        const float scales[]={1,1.25f,2};
        for(size_t i=0;i<3;++i) {
            xide_dpi_proxy* state=NULL;xui_proxy_t proxy={0};xui_font font=NULL;
            CHECK(xideDpiProxyCreate(&state,&proxy,scales[i])==XUI_OK);
            CHECK(proxy.fontLoadFile(&proxy,&font,argv[1],14,0)==XUI_OK);
            xui_text_item_t item={0};item.iSize=sizeof(item);item.pFont=font;
            item.sText="office 中文";item.iTextSize=-1;
            item.iFlags=XUI_TEXT_SHAPE_DEFAULT|XUI_TEXT_SHAPE_RETAIN_PAINT;
            logical=(xui_text_shape_t){0};logical.iSize=sizeof(logical);
            CHECK(proxy.textShape(&proxy,&item,&logical)==XUI_OK && logical.pPaint);
            CHECK(logical.pPaintOwnership && logical.pPaintOwnership->DropReady(logical.pPaint));
            proxy.fontDestroy(&proxy,font);xideDpiProxyDestroy(state);
            CHECK(xuiTextShapeFreeChecked(&logical)==XUI_OK && !logical.pPaint);
            CHECK(!logical.pClusters && !logical.pCarets && !xrtGetError());
        }
    } else CHECK(argc==1);
    puts("DPI paint move, trace, thread affinity, checked cleanup and OOM passed");return 0;
}
