/* The independent native suite and boundary/OOM checks use the actual SDK. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_radio_option.h"
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
static int changes,nullSelections;
static void change(xui_widget group,xui_widget selected,int index,void* user)
{
 (void)user;assert(group);++changes;
 if(!selected){assert(index==-1);++nullSelections;}else assert(xuiWidgetGetParent(selected)==group&&index>=0);
}
static int gone;
static void mark_gone(void* raw){(void)raw;gone=1;}
static void close_group(xui_widget group,xui_widget selected,int index,void* user)
{(void)selected;(void)index;(void)user;assert(xuiWidgetDestroyChecked(group)==XUI_OK);}
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 assert(xrtMemDebugEnable(true));xui_proxy proxy=NULL,other=NULL;xui_font font=NULL,wrong=NULL;xui_context ui=NULL;
 assert(fixtureLayoutProxy(&proxy)==XUI_OK&&fixtureLayoutFont(proxy,&font)==XUI_OK);
 assert(fixtureLayoutProxy(&other)==XUI_OK&&fixtureLayoutFont(other,&wrong)==XUI_OK);
 xui_context_desc_t options;xuiContextDescDefault(&options);options.iSystemFontPolicy=XUI_SYSTEM_FONT_DISABLED;
 assert(xuiCreateOwnedEx(proxy,&options,&ui)==XUI_OK&&xuiSetDefaultFont(ui,font)==XUI_OK);
 xui_widget group=NULL,first=NULL,second=NULL;assert(xuiRadioGroupCreate(ui,&group,NULL)==XUI_OK);
 assert(xuiRadioGroupSetChange(group,change,NULL)==XUI_OK);
 const xui_radio_desc_t full={.iSize=sizeof(full),.sText="first",.pFont=font,.iTextColor=UINT32_C(0x81223344),.iDisabledTextColor=UINT32_C(0x55667788),.iTextFlags=XUI_TEXT_ALIGN_RIGHT,.bChecked=1,.fIndicatorSize=19.5f,.fGap=7.25f,.bUseBuiltinAtlas=1};
 assert(xuiRadioGroupAddOption(group,&first,&full)==XUI_OK&&changes==0);
 assert(xuiWidgetGetParent(first)==group&&!strcmp(xuiRadioGetText(first),"first")&&xuiRadioGetFont(first)==font);
 assert(xuiRadioGetTextColor(first)==full.iTextColor&&xuiRadioGetDisabledTextColor(first)==full.iDisabledTextColor);
 assert((xuiRadioGetTextFlags(first)&XUI_TEXT_ALIGN_RIGHT)&&xuiRadioGetChecked(first)==1);
 assert(xuiRadioGetIndicatorSize(first)==19.5f&&xuiRadioGetGap(first)==7.25f&&xuiRadioGetUseBuiltinAtlas(first)==1);
 assert(xuiRadioGroupAddOption(group,&second,NULL)==XUI_OK&&!strcmp(xuiRadioGetText(second),""));
 assert(xuiRadioGroupSetSelectedIndex(group,1)==XUI_OK&&changes==1);
 assert(xuiRadioGroupSetSelectedIndex(group,-1)==XUI_OK&&changes==2&&nullSelections==1);
 assert(xuiRadioGroupSetChange(group,NULL,NULL)==XUI_OK&&xuiWidgetDestroyChecked(second)==XUI_OK&&xuiRadioGroupSetSelectedIndex(group,0)==XUI_OK);
 xui_radio_desc_t invalid=full;invalid.fIndicatorSize=-1;second=first;
 assert(xuiRadioGroupAddOption(group,&second,&invalid)==XUI_ERROR_INVALID_ARGUMENT&&second==NULL);
 invalid=full;invalid.pFont=wrong;assert(xuiRadioGroupAddOption(group,&second,&invalid)!=XUI_OK&&second==NULL&&xuiWidgetGetChildCount(group)==1);xrtClearError();
 const xui_radio_desc_t temporary={.iSize=sizeof(temporary),.sText="temporary"};
 unsigned failures=0,successes=0;
 for(int n=0;n<64;++n){fixtureOptionPrefixBegin();fixtureOptionBudget(n);int status=xuiRadioGroupAddOption(group,&second,&temporary);fixtureOptionBudget(-1);
  if(status==XUI_OK){++successes;assert(second&&xuiWidgetGetParent(second)==group);assert(xuiWidgetDestroyChecked(second)==XUI_OK);}
  else{++failures;assert(second==NULL);}
  assert(xuiWidgetGetChildCount(group)==1&&xuiRadioGroupGetSelectedIndex(group)==0);fixtureOptionPrefixEnd();fixtureOptionHeapEqual();
 }
 assert(failures&&successes);
 fixtureOptionNativeCreate(group);
 fixtureOptionHeapBegin();fixtureOptionNativeCreate(group);fixtureOptionHeapReport(1);
 fixtureOptionHeapBegin();fixtureOptionBudget(0);fixtureOptionNativeQueries(group,first);fixtureOptionBudget(-1);fixtureOptionHeapReport(3);
 xui_widget closing=NULL,child=NULL;assert(xuiRadioGroupCreate(ui,&closing,NULL)==XUI_OK&&xuiRadioGroupAddOption(closing,&child,NULL)==XUI_OK);
 xui_lifetime_link_t life={0};assert(xuiWidgetLifetimeWatch(closing,&life,mark_gone,NULL)==XUI_OK);
 assert(xuiRadioGroupSetChange(closing,close_group,NULL)==XUI_OK&&xuiRadioGroupSetSelectedIndex(closing,0)==XUI_OK&&gone);xuiLifetimeUnwatch(&life);
 assert(xuiDestroyChecked(ui)==XUI_OK&&xuiFontReleaseChecked(font)==XUI_OK&&xuiFontReleaseChecked(wrong)==XUI_OK&&xuiProxyReleaseChecked(other)==XUI_OK&&xuiProxyReleaseChecked(proxy)==XUI_OK);fixtureOptionCleanup();
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(!after.LiveCount&&!after.InvalidFreeCount&&!after.DoubleFreeCount);
 puts("RadioOption native: full descriptor, NULL selection, mismatched backend, atomic failures, close and heap cleanup passed");return 0;
}
