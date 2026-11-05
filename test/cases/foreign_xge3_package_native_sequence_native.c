/* Real owned XUI Shape, with a permanent CPU-only provider. No window/device. */
#include "test/cases/foreign_xge3_package_native_sequence.h"
#include <assert.h>
#include <string.h>
typedef struct Paint { xui_font font; } Paint;
static size_t references=1;
static unsigned shapes,shapeDrops,fontDrops;
static bool hold(const void* raw) { assert(raw==&references);++references;return true; }
static bool drop(const void* raw) { assert(raw==&references && references>1);--references;return true; }
static bool count(const void* raw,size_t* result) { assert(raw==&references);*result=references;return true; }
static bool trace(const void* raw,xrtownershipvisitor visit,void* user) { assert(raw==&references);(void)visit;(void)user;return true; }
static const xrtownershipops ownerOps={count,trace};
static int caps(xui_proxy proxy,xui_proxy_caps_t* out)
{ (void)proxy;*out=(xui_proxy_caps_t){.iSize=sizeof(*out),.iCaps=XUI_PROXY_CAP_TEXT};return XUI_OK; }
static int metrics(xui_proxy proxy,xui_font font,xui_font_metrics_t* out)
{ (void)proxy;assert(font);*out=(xui_font_metrics_t){.fSize=16,.fAscent=12,.fDescent=4,.fLineHeight=16};return XUI_OK; }
static int measure(xui_proxy proxy,const xui_text_item_t* item,xui_vec2_t* out)
{ (void)proxy;*out=(xui_vec2_t){item->iTextSize*8,16};return XUI_OK; }
static bool paintReady(const void* raw) { return raw!=NULL; }
static bool paintTrace(const void* raw,xrtownershipvisitor visit,void* user)
{ return visit(xuiFontOwnership(((const Paint*)raw)->font),user); }
static int paintDrop(void* raw)
{ Paint* paint=raw;++shapeDrops;int status=xuiFontReleaseChecked(paint->font);xrtFree(paint);return status; }
static const xui_text_paint_ownership_v1_t paintOps={sizeof(paintOps),XUI_TEXT_PAINT_OWNERSHIP_V1_TAG,paintReady,paintTrace,paintDrop};
static int shape(xui_proxy proxy,const xui_text_item_t* item,xui_text_shape_t* out)
{
 (void)proxy;assert(item && item->iTextSize>=0);
 bool empty=item->iTextSize==0;
 if(!empty)assert(item->iTextSize==4 && !memcmp(item->sText,"a\0bc",4));
 *out=(xui_text_shape_t){.iSize=sizeof(*out),.iTextSize=item->iTextSize,.iFlags=item->iFlags,
  .iClusterCount=empty ? 0:2,.iCaretCount=empty ? 0:2,.fWidth=empty ? 0:32,.fHeight=16,.fLineHeight=16};
 if(!empty){
  out->pClusters=xrtCalloc(2,sizeof(*out->pClusters));if(!out->pClusters)return XUI_ERROR_OUT_OF_MEMORY;
  out->pCarets=xrtCalloc(2,sizeof(*out->pCarets));if(!out->pCarets){xuiTextShapeFree(out);return XUI_ERROR_OUT_OF_MEMORY;}
  for(int i=0;i<2;++i){
   out->pClusters[i]=(xui_text_cluster_t){.iSize=sizeof(xui_text_cluster_t),.iTextStart=i*2,.iTextEnd=i*2+2,.fAdvance=16};
   out->pCarets[i]=(xui_text_caret_t){.iSize=sizeof(xui_text_caret_t),.iTextOffset=i*2+1,.fAdvance=8};
  }
 }
 Paint* paint=xrtMalloc(sizeof(*paint));if(!paint){xuiTextShapeFree(out);return XUI_ERROR_OUT_OF_MEMORY;}
 int status=xuiFontAddRef(item->pFont);if(status!=XUI_OK){xrtFree(paint);xuiTextShapeFree(out);return status;}
 paint->font=item->pFont;out->pPaint=paint;out->pPaintOwnership=&paintOps;++shapes;return XUI_OK;
}
static int finalizeFont(xui_proxy proxy,void* raw) { (void)proxy;(void)raw;++fontDrops;return XUI_OK; }
int sequence_cpu_proxy(xui_proxy* out)
{
 const xui_proxy_t table={.iSize=sizeof(table),.iVersion=XUI_PROXY_VERSION,.getCaps=caps,.fontGetMetrics=metrics,.textMeasure=measure,.textShape=shape};
 const xui_callback_owner_t owner={{&references,&ownerOps},hold,drop};return xuiProxyCreateOwned(&table,&owner,out);
}
int sequence_cpu_font(xui_proxy proxy,xui_font* out) { void* payload=NULL;return xuiFontCreateOwned(proxy,0,finalizeFont,out,&payload); }
int sequence_cpu_shape_drops(void) { return (int)shapeDrops; }
int sequence_cpu_font_drops(void) { return (int)fontDrops; }
void sequence_cpu_no_alloc(void) { assert(xrtMemDebugFailAfter(0)); }
int sequence_cpu_reset_alloc(void) { bool hit=xrtMemDebugFailTriggered();assert(xrtMemDebugFailAfter(-1));return hit; }
int sequence_cpu_verify(void) { return shapes==2 && shapeDrops==2 && fontDrops==1 && references==1 && !xl6ForeignViewPendingV1(0); }
