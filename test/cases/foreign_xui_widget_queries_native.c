/* The independent native suite and boundary/OOM checks use the actual SDK. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_widget_queries.h"
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
#include <limits.h>
#include <stdio.h>
static int fail_measure(xui_widget widget,xui_vec2_t constraint,xui_vec2_t* result,void* user)
{(void)widget;(void)constraint;(void)user;result->fX=123;return XUI_ERROR_INVALID_STATE;}
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 xui_proxy proxy=NULL;xui_context ui=NULL;xui_font font=NULL;
 assert(fixtureLayoutProxy(&proxy)==XUI_OK);
 xui_context_desc_t desc;xuiContextDescDefault(&desc);desc.iSystemFontPolicy=XUI_SYSTEM_FONT_DISABLED;
 assert(xuiCreateOwnedEx(proxy,&desc,&ui)==XUI_OK);
 assert(fixtureLayoutFont(proxy,&font)==XUI_OK&&xuiSetDefaultFont(ui,font)==XUI_OK);
 xui_widget widget=NULL,terminal=NULL;assert(xuiWidgetCreate(ui,&widget)==XUI_OK&&xuiTerminalCreate(ui,&terminal,NULL)==XUI_OK);
 assert(xuiTerminalResize(terminal,20,4)==XUI_OK);
 assert(xuiTerminalWriteText(terminal,"A\xcc\x81 \xe4\xb8\xad\nB")==XUI_OK&&xuiTerminalFlush(terminal)==XUI_OK);
 xui_terminal_cell_t cell={0};
 assert(xuiTerminalGetCell(terminal,0,0,&cell)==XUI_OK&&cell.iSize==96&&cell.iCodepoint==65&&cell.iWidth==1&&cell.iCombiningCount==1&&cell.arrCombining[0]==0x301);
 for(int i=1;i<16;++i)assert(cell.arrCombining[i]==0);
 assert(xuiTerminalGetCell(terminal,2,0,&cell)==XUI_OK&&cell.iCodepoint==0x4e2d&&cell.iWidth==2);
 assert(xuiTerminalGetCell(terminal,3,0,&cell)==XUI_OK&&cell.iWidth==0);
 cell.iCodepoint=99;assert(xuiTerminalGetCell(terminal,-1,0,&cell)==XUI_ERROR_INVALID_ARGUMENT&&cell.iCodepoint==99);
 xui_vec2_t value={99,77};
 assert(xuiWidgetMeasureContent(widget,(xui_vec2_t){-1,0},&value)==XUI_ERROR_INVALID_ARGUMENT&&value.fX==99&&value.fY==77);
 assert(xuiWidgetMeasure(widget,(xui_vec2_t){-1,0},&value)==XUI_ERROR_INVALID_ARGUMENT&&value.fX==99&&value.fY==77);
 assert(xuiWidgetSetContentMeasureCallback(widget,fail_measure,NULL)==XUI_OK);
 assert(xuiWidgetMeasureContent(widget,(xui_vec2_t){500,300},&value)==XUI_ERROR_INVALID_STATE&&value.fX==99&&value.fY==77);
 assert(xuiWidgetSetContentMeasureCallback(widget,NULL,NULL)==XUI_OK);
 fixtureQueryWarm(widget);fixtureQueryHeapBegin();fixtureQueryBudget(0);
 fixtureQueryNativeHot(widget,terminal);fixtureQueryBudget(-1);fixtureQueryHeapReport(1);
 assert(xuiDestroyChecked(ui)==XUI_OK);
 assert(xuiFontReleaseChecked(font)==XUI_OK&&xuiProxyReleaseChecked(proxy)==XUI_OK);
 fixtureQueryCleanup();puts("WidgetQueries native: bounds, full terminal cell, combining arrays, measurement failure and warm heap passed");return 0;
}
