/* Deterministic CPU backend, used identically by all comparison columns.
 * Real production XUI owns Proxy/Font/Surface/Draw/Shape controls. No GPU,
 * browser, OS clipboard or window is created. Counter mode is a separate
 * binary; timing mode contains only the workload checksum side effect. */
#include "binding_perf.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
static size_t provider_refs=1;
static uint64_t checksum;
#ifdef BINDING_PERF_COUNT
static uint64_t holds,drops,caps_calls,draw_calls,shape_calls;
static xmemdebugsnapshot before;
static bool counters_enabled;
static uint64_t old_holds,old_drops,old_caps,old_draws,old_shapes;
#define COUNT(x) (++(x))
#else
#define COUNT(x) ((void)0)
#endif
static bool providerHold(const void* raw)
{assert(raw==&provider_refs);++provider_refs;COUNT(holds);return true;}
static bool providerDrop(const void* raw)
{assert(raw==&provider_refs && provider_refs>1);--provider_refs;COUNT(drops);return true;}
static bool providerCount(const void* raw,size_t* output)
{assert(raw==&provider_refs);*output=provider_refs;return true;}
static bool providerTrace(const void* raw,xrtownershipvisitor visit,void* user)
{assert(raw==&provider_refs);(void)visit;(void)user;return true;}
static const xrtownershipops providerOps={providerCount,providerTrace};
static int getCaps(xui_proxy proxy,xui_proxy_caps_t* output)
{
 (void)proxy;COUNT(caps_calls);
 *output=(xui_proxy_caps_t){.iSize=sizeof(*output),.iCaps=XUI_PROXY_CAP_TEXT,
   .iSurfaceFormat=7,.iInternalAlpha=3};
 return XUI_OK;
}
static int fillRect(xui_proxy proxy,xui_draw_context draw,xui_rect_t rect,uint32_t color)
{
 assert(proxy && draw);COUNT(draw_calls);
 checksum+=color+(uint64_t)rect.fX;
 return XUI_OK;
}
static int fontMetrics(xui_proxy proxy,xui_font font,xui_font_metrics_t* output)
{
 (void)proxy;assert(font);
 *output=(xui_font_metrics_t){.fSize=16,.fAscent=12,.fDescent=4,.fLineHeight=16};
 return XUI_OK;
}
typedef struct PerfPaint {xui_font font;} PerfPaint;
static bool paintReady(const void* raw){return raw!=NULL;}
static bool paintTrace(const void* raw,xrtownershipvisitor visit,void* user)
{return visit(xuiFontOwnership(((const PerfPaint*)raw)->font),user);}
static int paintDrop(void* raw)
{
 PerfPaint* paint=raw;int status=xuiFontReleaseChecked(paint->font);xrtFree(paint);return status;
}
static const xui_text_paint_ownership_v1_t paintOps={sizeof(paintOps),XUI_TEXT_PAINT_OWNERSHIP_V1_TAG,paintReady,paintTrace,paintDrop};
static int shapeText(xui_proxy proxy,const xui_text_item_t* item,xui_text_shape_t* output)
{
 (void)proxy;COUNT(shape_calls);
 if(!item || item->iTextSize<0 || !output)return XUI_ERROR_INVALID_ARGUMENT;
 const size_t n=(size_t)item->iTextSize/2;
 *output=(xui_text_shape_t){.iSize=sizeof(*output),.iTextSize=item->iTextSize,
   .iFlags=item->iFlags,.fLineHeight=16,.fWidth=(float)n*16,.fHeight=16};
 if(n){
  output->pClusters=xrtCalloc(n,sizeof(*output->pClusters));
  output->pCarets=xrtCalloc(n,sizeof(*output->pCarets));
  if(!output->pClusters || !output->pCarets){xuiTextShapeFree(output);return XUI_ERROR_OUT_OF_MEMORY;}
 }
 output->iClusterCount=(int)n;output->iCaretCount=(int)n;
 for(size_t i=0;i<n;++i){
  output->pClusters[i]=(xui_text_cluster_t){.iSize=sizeof(xui_text_cluster_t),
    .iTextStart=(int)i*2,.iTextEnd=(int)i*2+2,.fAdvance=16};
  output->pCarets[i]=(xui_text_caret_t){.iSize=sizeof(xui_text_caret_t),
    .iTextOffset=(int)i*2+1,.fAdvance=8};
 }
 PerfPaint* paint=xrtMalloc(sizeof(*paint));if(!paint){xuiTextShapeFree(output);return XUI_ERROR_OUT_OF_MEMORY;}
 int status=xuiFontAddRef(item->pFont);if(status!=XUI_OK){xrtFree(paint);xuiTextShapeFree(output);return status;}
 paint->font=item->pFont;output->pPaint=paint;output->pPaintOwnership=&paintOps;output->iPaintBytes=sizeof(*paint);
 return XUI_OK;
}
static int finalized(xui_proxy proxy,void* payload)
{(void)proxy;(void)payload;return XUI_OK;}
int binding_perf_proxy(xui_proxy* output)
{
 const xui_proxy_t table={.iSize=sizeof(table),.iVersion=XUI_PROXY_VERSION,
   .getCaps=getCaps,.drawRectFill=fillRect,.fontGetMetrics=fontMetrics,.textShape=shapeText};
 const xui_callback_owner_t owner={{&provider_refs,&providerOps},providerHold,providerDrop};
 return xuiProxyCreateOwned(&table,&owner,output);
}
int binding_perf_font(xui_proxy proxy,xui_font* output)
{void* payload=NULL;return xuiFontCreateOwned(proxy,0,finalized,output,&payload);}
int binding_perf_surface(xui_proxy proxy,xui_surface* output)
{void* payload=NULL;return xuiSurfaceCreateOwned(proxy,0,finalized,output,&payload);}
int binding_perf_draw(xui_proxy proxy,xui_surface target,xui_draw_context* output)
{void* payload=NULL;return xuiDrawCreateOwned(proxy,target,0,finalized,output,&payload);}
int binding_perf_shape(xui_proxy proxy,xui_font font,const xui_text_shape_t** output)
{
 /* Embedded NUL bytes do not affect this deterministic backend's records.
  * Its explicit count is exact. The production Shape owns the backend arrays. */
 static const char text[8192]={0};
 const xui_text_item_t item={.iSize=sizeof(item),.pFont=font,.sText=text,.iTextSize=sizeof(text)};
 return xuiTextShapeCreateOwned(proxy,&item,output);
}
unsigned long long binding_perf_checksum(void){return checksum;}
unsigned long long binding_perf_clock(void)
{
#ifdef _WIN32
 LARGE_INTEGER value;assert(QueryPerformanceCounter(&value));return (uint64_t)value.QuadPart;
#else
 struct timespec value;assert(!clock_gettime(CLOCK_MONOTONIC,&value));
 return (uint64_t)value.tv_sec*UINT64_C(1000000000)+(uint64_t)value.tv_nsec;
#endif
}
unsigned long long binding_perf_frequency(void)
{
#ifdef _WIN32
 LARGE_INTEGER value;assert(QueryPerformanceFrequency(&value));return (uint64_t)value.QuadPart;
#else
 return UINT64_C(1000000000);
#endif
}
int binding_perf_verify(void){return provider_refs==1;}
void binding_perf_counter_begin(void)
{
#ifdef BINDING_PERF_COUNT
 if(!counters_enabled){assert(xrtMemDebugEnable(true));counters_enabled=true;}
 xrtMemDebugSnapshot(&before);
 old_holds=holds;old_drops=drops;old_caps=caps_calls;old_draws=draw_calls;old_shapes=shape_calls;
#endif
}
void binding_perf_counter_end(int tag)
{
#ifdef BINDING_PERF_COUNT
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("counter %d alloc=%llu free=%llu realloc=%llu liveCount=%lld liveBytes=%lld processPeakCount=%zu processPeakBytes=%zu holds=%llu drops=%llu caps=%llu draws=%llu shapes=%llu\n",
  tag,(unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
  (unsigned long long)(after.ReallocCount-before.ReallocCount),(long long)after.LiveCount-(long long)before.LiveCount,
  (long long)after.LiveBytes-(long long)before.LiveBytes,after.PeakCount,after.PeakBytes,
  (unsigned long long)(holds-old_holds),(unsigned long long)(drops-old_drops),(unsigned long long)(caps_calls-old_caps),
  (unsigned long long)(draw_calls-old_draws),(unsigned long long)(shape_calls-old_shapes));
 /* End also establishes the next observation baseline. The final host End
  * follows function/module cleanup without a language-side Begin, so it
  * must not count the preceding explicit-close phase a second time. */
 before=after;
 old_holds=holds;old_drops=drops;old_caps=caps_calls;old_draws=draw_calls;old_shapes=shape_calls;
#else
 (void)tag;
#endif
}
