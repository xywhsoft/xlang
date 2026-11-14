/* Independent native callers use exactly the same O2 SDK and CPU provider. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_menu_items.h"
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
static int exercise(int kind,int allowance)
{
 xui_proxy proxy=NULL;xui_font font=NULL;xui_context ui=create(&proxy,&font);
 xui_widget menu=NULL,child=NULL;assert(xuiMenuCreate(ui,&menu,NULL)==XUI_OK&&xuiMenuCreate(ui,&child,NULL)==XUI_OK);
 xui_menu_item_t first={.sText="kept",.sShortcut="Ctrl+K",.iState=1,.iValue=1};assert(xuiMenuAddItem(menu,&first)==XUI_OK);
 xui_menu_item_t values[3]={
  {.sText="Run",.sShortcut="Ctrl+R",.iType=2,.iState=7,.iValue=11,.iIcon=7},
  {.sText="More",.iType=4,.iState=1,.iValue=22,.iIcon=9,.pSubmenu=child},
  {.sText="",.sShortcut="",.iType=3,.iState=9,.iValue=33}};
 if(allowance<0)fixtureMenuBoundBegin();else fixtureRenderBudget(allowance);
 int status=kind==0?xuiMenuSetItems(menu,values,3):xuiMenuAddItem(menu,&values[1]);
 fixtureRenderBudget(-1);int count=allowance<0?fixtureMenuBoundEnd():0;
 if(allowance<0)assert(status==XUI_OK);
 xrtClearError();fixtureMenuCheck(menu,status==XUI_OK?2-kind:0,child);
 assert(!strcmp(values[0].sText,"Run")&&!strcmp(values[0].sShortcut,"Ctrl+R")&&values[1].pSubmenu==child);
 finish(ui,proxy,font);return allowance<0?count:status==XUI_OK?0:1;
}
static void functional(void)
{
 xui_proxy proxy=NULL;xui_font font=NULL;xui_context ui=create(&proxy,&font);
 xui_widget menu=NULL,child=NULL,parent=NULL;assert(xuiMenuCreate(ui,&menu,NULL)==XUI_OK&&xuiMenuCreate(ui,&child,NULL)==XUI_OK&&xuiMenuCreate(ui,&parent,NULL)==XUI_OK);
 xui_menu_item_t nullable[2]={{.iType=1},{.sText="",.sShortcut="",.iState=1}};
 assert(xuiMenuSetItems(menu,nullable,2)==XUI_OK);fixtureMenuNullCheck(menu);
 assert(xuiMenuSetItems(menu,NULL,0)==XUI_OK&&xuiMenuGetItemCount(menu)==0);
 assert(xuiMenuSetItems(menu,nullable,-1)==XUI_ERROR_INVALID_ARGUMENT&&xuiMenuSetItems(menu,NULL,1)==XUI_ERROR_INVALID_ARGUMENT&&xuiMenuSetItems(menu,nullable,65)==XUI_ERROR_INVALID_ARGUMENT);
 xui_menu_item_t more={.sText="child",.iType=4,.iState=1,.pSubmenu=child};
 more.pSubmenu=menu;assert(xuiMenuAddItem(menu,&more)==XUI_ERROR_INVALID_ARGUMENT);
 more.pSubmenu=NULL;assert(xuiMenuAddItem(menu,&more)==XUI_ERROR_INVALID_ARGUMENT);
 xui_context_desc_t desc;xuiContextDescDefault(&desc);desc.iSystemFontPolicy=XUI_SYSTEM_FONT_DISABLED;xui_context other=NULL;
 assert(xuiCreateOwnedEx(proxy,&desc,&other)==XUI_OK&&xuiSetDefaultFont(other,font)==XUI_OK);
 xui_widget foreign=NULL;assert(xuiMenuCreate(other,&foreign,NULL)==XUI_OK);more.pSubmenu=foreign;
 assert(xuiMenuAddItem(menu,&more)==XUI_ERROR_INVALID_ARGUMENT&&xuiDestroyChecked(other)==XUI_OK);
 more.pSubmenu=child;assert(xuiMenuAddItem(menu,&more)==XUI_OK&&xuiMenuAddItem(parent,&more)==XUI_OK);
 assert(xuiWidgetDestroyChecked(parent)==XUI_OK&&xuiMenuGetItemCount(child)==0);
 assert(xuiWidgetDestroyChecked(child)==XUI_OK);
 const xui_menu_item_t* item=xuiMenuGetItem(menu,0);assert(item&&item->pSubmenu==NULL&&!(item->iState&XUI_MENU_ITEM_ENABLED));
 more.pSubmenu=NULL;more.iType=0;more.sText="self copy";more.sShortcut="Ctrl+S";
 assert(xuiMenuSetItems(menu,&more,1)==XUI_OK);fixtureMenuSelfCheck(menu);
 finish(ui,proxy,font);
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
 for(int kind=0;kind<2;++kind){
  int count=exercise(kind,-1);assert(exercise(kind,0)==1);unsigned failures=0,successes=0;
  for(int n=0;n<count;++n){fixtureMenuHeapBegin();if(exercise(kind,n))++failures;else ++successes;fixtureMenuHeapEqual();}
  assert(failures&&successes);printf("Native menu kind=%d prefixes=%d\n",kind,count);
 }
 xmemdebugsnapshot end;xrtMemDebugSnapshot(&end);assert(!end.LiveCount&&!end.InvalidFreeCount&&!end.DoubleFreeCount);
 puts("MenuItems native: fields, nullable strings, atomic OOM, caller storage, alias copies and weak submenu retirement passed");return 0;
}
