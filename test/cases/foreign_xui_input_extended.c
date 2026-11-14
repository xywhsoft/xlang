/* CPU backend only; actual widget/input implementations come from the SDK. */
#include "foreign_xui_input_extended.h"
#include "foreign_xui_text_layout.c"
static int inputShape(xui_proxy proxy,const xui_text_item_t* item,xui_text_shape_t* output)
{
 (void)proxy;int n=item->iTextSize<0?(int)strlen(item->sText):item->iTextSize,at=0,count=0;
 while(at<n){unsigned char c=(unsigned char)item->sText[at];at+=c<0x80?1:c<0xe0?2:c<0xf0?3:4;++count;}
 *output=(xui_text_shape_t){.iSize=sizeof(*output),.iTextSize=n,.iFlags=item->iFlags,.fAscent=12,.fDescent=4,.fLineHeight=16,.fHeight=16,.fWidth=count*8};
 if(count){output->pClusters=xrtCalloc(count,sizeof(*output->pClusters));if(!output->pClusters)return XUI_ERROR_OUT_OF_MEMORY;}
 output->iClusterCount=count;at=0;
 for(int i=0;i<count;++i){unsigned char c=(unsigned char)item->sText[at];int end=at+(c<0x80?1:c<0xe0?2:c<0xf0?3:4);if(end>n)end=n;
  output->pClusters[i]=(xui_text_cluster_t){.iSize=sizeof(xui_text_cluster_t),.iTextStart=at,.iTextEnd=end,.fAdvance=8};at=end;}
 Paint* paint=xrtMalloc(sizeof(*paint));if(!paint){xuiTextShapeFree(output);return XUI_ERROR_OUT_OF_MEMORY;}
 int status=xuiFontAddRef(item->pFont);if(status!=XUI_OK){xrtFree(paint);xuiTextShapeFree(output);return status;}
 paint->font=item->pFont;output->pPaint=paint;output->pPaintOwnership=&paintOps;++shapes;return XUI_OK;
}
int fixtureInputProxy(xui_proxy* output)
{
 xuiTestProxyInit(&backend);backend.tProxy.fontGetMetrics=metrics;backend.tProxy.textMeasure=measure;
 backend.tProxy.textShape=inputShape;backend.tProxy.textDraw=draw;
 const xui_callback_owner_t owner={{&references,&operations},hold,drop};return xuiProxyCreateOwned(&backend.tProxy,&owner,output);
}
void fixtureInputCleanupCheck(void){assert(references==1&&fonts==1&&paints==shapes&&surfaces==0&&draws==0);}
void fixtureInputField(int count,int (*callback)(const xui_event_t*,void*),void* user)
{
 xui_event_t event={0};event.iSize=sizeof(event);memset(event.sText,'x',sizeof(event.sText));
 event.sText[0]='a';event.sText[1]=0;event.sText[2]='b';event.iTextSize=count;
 assert(callback);(void)callback(&event,user);
}
void fixtureInputBudget(int n){fixtureLayoutBudget(n);}
static xmemdebugsnapshot inputBefore;
void fixtureInputHeapBegin(void){assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&inputBefore);}
void fixtureInputHeapReport(int kind)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 printf("InputEx heap kind=%d alloc=%llu free=%llu realloc=%llu\n",kind,
  (unsigned long long)(after.AllocCount-inputBefore.AllocCount),(unsigned long long)(after.FreeCount-inputBefore.FreeCount),(unsigned long long)(after.ReallocCount-inputBefore.ReallocCount));
}
int fixtureInputTextCapacity(void){return XUI_EVENT_TEXT_CAPACITY;}
void fixtureInputNativeHot(xui_context context)
{
 const xui_ime_composition_t composition={sizeof(composition),"ab",2,1,2,0,2,0,0,0,0};
 for(int i=0;i<1000;++i){uint32_t flags=37;
  assert(xuiInputKeyDownEx(context,'A',0,&flags)==XUI_OK&&flags==0);
  assert(xuiInputKeyUpEx(context,'A',0,&flags)==XUI_OK&&flags==0);
  assert(xuiInputTextEx(context,'A',&flags)==XUI_OK&&flags==0);
  assert(xuiInputImeCompositionEx(context,&composition)==XUI_OK);xuiClearEvents(context);
 }
}

static int inputErrorCount;
static void inputError(xui_context context,const xui_error_info_t* error,void* user)
{(void)context;(void)user;assert(error && error->iCode==XUI_ERROR_INVALID_ARGUMENT);++inputErrorCount;}
void fixtureInputObserveErrors(xui_context context){assert(xuiSetErrorCallback(context,inputError,NULL)==XUI_OK);}
int fixtureInputErrorCount(void){return inputErrorCount;}
