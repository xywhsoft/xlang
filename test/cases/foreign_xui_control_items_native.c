/* Independent native callers use exactly the same O2 SDK and CPU provider. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_control_items.h"
#if defined(XGE_TEST_STATIC)
#include <xge_static_xrt_contract.h>
#else
#include <xge_host_xrt_contract.h>
static xge_host_xrt_proc resolve(void* user,const char* name)
{
 (void)user;
#define XGE_HOST_XRT_SLOT(symbol) if(!strcmp(name,#symbol))return (xge_host_xrt_proc)&symbol;
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
 return NULL;
}
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
static xui_context create(xui_proxy* proxy,xui_font* font)
{
 assert(fixtureRenderProxy(proxy)==XUI_OK&&fixtureRenderFont(*proxy,font)==XUI_OK);
 xui_context_desc_t desc;xuiContextDescDefault(&desc);desc.iSystemFontPolicy=XUI_SYSTEM_FONT_DISABLED;
 xui_context ui=NULL;assert(xuiCreateOwnedEx(*proxy,&desc,&ui)==XUI_OK&&xuiSetDefaultFont(ui,*font)==XUI_OK);return ui;
}
static void finish(xui_context ui,xui_proxy proxy,xui_font font)
{
 assert(xuiDestroyChecked(ui)==XUI_OK&&xuiFontReleaseChecked(font)==XUI_OK&&xuiProxyReleaseChecked(proxy)==XUI_OK);
 assert(fixtureRenderBalanced());
}
static xui_widget widgetCreate(int kind,xui_context ui)
{
 xui_widget value=NULL;int status=kind==0?xuiBreadcrumbCreate(ui,&value,NULL):kind==1?xuiMenuBarCreate(ui,&value,NULL):xuiStatusBarCreate(ui,&value,NULL);assert(status==XUI_OK&&value);return value;
}
static int exercise(int kind,int allowance)
{
 xui_proxy proxy=NULL;xui_font font=NULL;xui_context ui=create(&proxy,&font);xui_widget widget=widgetCreate(kind,ui),child=NULL;assert(xuiMenuCreate(ui,&child,NULL)==XUI_OK);fixtureControlSeed(kind,widget,child);
 if(allowance<0)fixtureControlBoundBegin();else fixtureRenderBudget(allowance);
 int status=fixtureControlNativeSet(kind,widget,child);fixtureRenderBudget(-1);int count=allowance<0?fixtureControlBoundEnd():0;if(allowance<0)assert(status==XUI_OK);xrtClearError();fixtureControlCheck(kind,widget,child,status==XUI_OK);finish(ui,proxy,font);return allowance<0?count:status==XUI_OK?0:1;
}
static void functional(void)
{
 xui_proxy proxy=NULL;xui_font font=NULL;xui_context ui=create(&proxy,&font);xui_widget crumb=widgetCreate(0,ui),bar=widgetCreate(1,ui),status=widgetCreate(2,ui),child=NULL;assert(xuiMenuCreate(ui,&child,NULL)==XUI_OK);
 assert(fixtureControlNativeSet(0,crumb,child)==XUI_OK&&fixtureControlNativeSet(1,bar,child)==XUI_OK&&fixtureControlNativeSet(2,status,child)==XUI_OK);for(int k=0;k<3;++k)fixtureControlCheck(k,k==0?crumb:k==1?bar:status,child,1);
 const char* label=xuiBreadcrumbGetItemText(crumb,0);xui_breadcrumb_item_t c={label,1,12};assert(xuiBreadcrumbSetItems(crumb,&c,1)==XUI_OK&&!strcmp(xuiBreadcrumbGetItemText(crumb,0),"Home")&&xuiBreadcrumbGetItemValue(crumb,0)==12);
 const xui_menubar_item_t* menus=xuiMenuBarGetItem(bar,0);assert(xuiMenuBarSetItems(bar,menus,2)==XUI_OK&&xuiMenuBarGetItemCount(bar)==2&&!strcmp(xuiMenuBarGetItem(bar,0)->sText,"&File"));
 const xui_statusbar_item_t* items=xuiStatusBarGetItem(status,0);assert(xuiStatusBarSetItems(status,items,2)==XUI_OK&&xuiStatusBarGetItemCount(status)==2&&!strcmp(xuiStatusBarGetItem(status,0)->sText,"Ready"));
 assert(xuiBreadcrumbSetItems(crumb,NULL,1)==XUI_ERROR_INVALID_ARGUMENT&&xuiBreadcrumbSetItems(crumb,&c,33)==XUI_ERROR_INVALID_ARGUMENT);
 assert(xuiMenuBarSetItems(bar,NULL,1)==XUI_ERROR_INVALID_ARGUMENT&&xuiMenuBarSetItems(bar,menus,-1)==XUI_ERROR_INVALID_ARGUMENT);
 assert(xuiStatusBarSetItems(status,NULL,1)==XUI_ERROR_INVALID_ARGUMENT&&xuiStatusBarSetItems(status,items,33)==XUI_ERROR_INVALID_ARGUMENT);
 xui_statusbar_item_t invalid={.sText="bad",.iType=99};assert(xuiStatusBarSetItems(status,&invalid,1)==XUI_ERROR_INVALID_ARGUMENT&&xuiStatusBarGetItemCount(status)==2);
 assert(fixtureControlNativeSet(1,bar,child)==XUI_OK);assert(xuiWidgetDestroyChecked(child)==XUI_OK);fixtureControlCheck(1,bar,NULL,2);
 assert(xuiBreadcrumbSetItems(crumb,NULL,0)==XUI_OK&&xuiMenuBarSetItems(bar,NULL,0)==XUI_OK&&xuiStatusBarSetItems(status,NULL,0)==XUI_OK);finish(ui,proxy,font);
}
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 assert(xrtMemDebugEnable(true));functional();
 for(int kind=0;kind<3;++kind){
  int count=exercise(kind,-1);assert(exercise(kind,0)==1);unsigned failures=0,successes=0;
  for(int n=0;n<count;++n){fixtureControlHeapBegin();if(exercise(kind,n))++failures;else ++successes;fixtureControlHeapEqual();}
  assert(failures&&successes);printf("Native control item kind=%d prefixes=%d failures=%u\n",kind,count,failures);
 }
 xmemdebugsnapshot end;xrtMemDebugSnapshot(&end);assert(!end.LiveCount&&!end.InvalidFreeCount&&!end.DoubleFreeCount);
 puts("ControlItems native: exact fields, copied/aliased text, normalized values, OOM rollback and weak menus passed");return 0;
}
