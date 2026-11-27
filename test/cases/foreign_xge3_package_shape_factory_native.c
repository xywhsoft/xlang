/* A permanent CPU host provider: its static table/userdata and executable code
 * live for the entire process. No device, OS clipboard, or desktop is used. */
#include "test/cases/foreign_xge3_package_shape_factory.h"
#include <__xl_runtime/xl6_foreign_view.h>
#include <assert.h>
#include <string.h>
typedef struct Paint {xui_font font;} Paint;
static bool paintReady,armed;
static int partialStatus;
static unsigned shapes,paintDrops,fontDrops;
static size_t hostReferences=1;
static bool hostHold(const void* raw){assert(raw==&hostReferences);++hostReferences;return true;}
static bool hostDrop(const void* raw){assert(raw==&hostReferences && hostReferences>1);--hostReferences;return true;}
static bool hostCount(const void* raw,size_t* output){assert(raw==&hostReferences);*output=hostReferences;return true;}
static bool hostTrace(const void* raw,xrtownershipvisitor visit,void* user){assert(raw==&hostReferences);(void)visit;(void)user;return true;}
static const xrtownershipops hostOps={hostCount,hostTrace};
static int caps(xui_proxy proxy,xui_proxy_caps_t* output)
{(void)proxy;*output=(xui_proxy_caps_t){.iSize=sizeof(*output),.iCaps=XUI_PROXY_CAP_TEXT};return XUI_OK;}
static int fontMetrics(xui_proxy proxy,xui_font font,xui_font_metrics_t* output)
{(void)proxy;assert(font);*output=(xui_font_metrics_t){.fSize=16,.fAscent=12,.fDescent=4,.fLineHeight=16};return XUI_OK;}
static int measure(xui_proxy proxy,const xui_text_item_t* item,xui_vec2_t* output)
{(void)proxy;*output=(xui_vec2_t){item->iTextSize*8,16};return XUI_OK;}
static bool readyPaint(const void* raw){(void)raw;return paintReady;}
static bool tracePaint(const void* raw,xrtownershipvisitor visit,void* user)
{return visit(xuiFontOwnership(((const Paint*)raw)->font),user);}
static int dropPaint(void* raw)
{Paint* paint=raw;++paintDrops;int status=xuiFontReleaseChecked(paint->font);xrtFree(paint);return status;}
static const xui_text_paint_ownership_v1_t paintOps={sizeof(paintOps),XUI_TEXT_PAINT_OWNERSHIP_V1_TAG,readyPaint,tracePaint,dropPaint};
static int shapeText(xui_proxy proxy,const xui_text_item_t* item,xui_text_shape_t* output)
{
 (void)proxy;assert(armed && item->iTextSize==3 && !memcmp(item->sText,"a\0b",3));
 *output=(xui_text_shape_t){.iSize=sizeof(*output),.iTextSize=3,.iFlags=item->iFlags,.iClusterCount=3,.fLineHeight=16,.fWidth=24,.fHeight=16};
 output->pClusters=xrtCalloc(3,sizeof(*output->pClusters));if(!output->pClusters)return XUI_ERROR_OUT_OF_MEMORY;
 Paint* paint=xrtMalloc(sizeof(*paint));if(!paint){xuiTextShapeFree(output);return XUI_ERROR_OUT_OF_MEMORY;}
 int result=xuiFontAddRef(item->pFont);if(result!=XUI_OK){xrtFree(paint);xuiTextShapeFree(output);return result;}
 paint->font=item->pFont;output->pPaint=paint;output->pPaintOwnership=&paintOps;++shapes;
 /* Native already allocated the actual result and all its payload here.
  * Any subsequent language root allocation would now fail. */
 assert(xrtMemDebugFailAfter(0));armed=false;return partialStatus;
}
static int finalizeFont(xui_proxy proxy,void* raw)
{(void)proxy;(void)raw;++fontDrops;return XUI_OK;}
int shape_cpu_proxy(xui_proxy* output)
{
 const xui_proxy_t table={.iSize=sizeof(table),.iVersion=XUI_PROXY_VERSION,.getCaps=caps,
   .fontGetMetrics=fontMetrics,.textMeasure=measure,.textShape=shapeText};
 const xui_callback_owner_t owner={{&hostReferences,&hostOps},hostHold,hostDrop};
 return xuiProxyCreateOwned(&table,&owner,output);
}
int shape_cpu_font(xui_proxy proxy,xui_font* output)
{void* payload=NULL;return xuiFontCreateOwned(proxy,0,finalizeFont,output,&payload);}
void shape_cpu_arm(int partial)
{assert(!armed);partialStatus=partial ? XUI_ERROR_BACKEND_FAILED:XUI_OK;paintReady=false;armed=true;}
int shape_cpu_observed(void){return (int)shapes;}
int shape_cpu_clear_oom(void)
{bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();return triggered;}
void shape_cpu_ready(void){paintReady=true;}
int shape_cpu_finish(void)
{assert(xl6ForeignViewDrainV1() && !xl6ForeignViewPendingV1(0));return paintDrops==shapes ? XUI_OK:XUI_ERROR_INVALID_STATE;}
int shape_cpu_verify(void)
{return shapes==2 && paintDrops==2 && fontDrops==1 && hostReferences==1 && !xl6ForeignViewPendingV1(0);}
