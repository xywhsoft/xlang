/* The independent native suite and boundary/OOM checks use the actual SDK. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_tag_batch.h"
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
static int changes;
static void count_change(xui_widget widget,int count,void* user)
{(void)widget;(void)count;(void)user;++changes;}
static void close_change(xui_widget widget,int count,void* user)
{(void)count;(void)user;assert(xuiWidgetDestroyChecked(widget)==XUI_OK);}
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 assert(xrtMemDebugEnable(true));xui_proxy proxy=NULL;xui_font font=NULL;xui_context ui=NULL;
 assert(fixtureLayoutProxy(&proxy)==XUI_OK&&fixtureLayoutFont(proxy,&font)==XUI_OK);
 xui_context_desc_t desc;xuiContextDescDefault(&desc);desc.iSystemFontPolicy=XUI_SYSTEM_FONT_DISABLED;
 assert(xuiCreateOwnedEx(proxy,&desc,&ui)==XUI_OK&&xuiSetDefaultFont(ui,font)==XUI_OK);
 xui_widget tags=NULL;assert(xuiTagInputCreate(ui,&tags,NULL)==XUI_OK);
 const char* values[]={" a ","b","a"};assert(xuiTagInputSetChange(tags,count_change,NULL)==XUI_OK);
 assert(xuiTagInputSetTags(tags,values,3)==XUI_OK&&changes==1&&xuiTagInputGetTagCount(tags)==3);
 assert(!strcmp(xuiTagInputGetTag(tags,0),"a")&&!strcmp(xuiTagInputGetTag(tags,2),"a"));
 assert(xuiTagInputSetTags(tags,values,3)==XUI_OK&&changes==1);
 assert(xuiTagInputSetChange(tags,NULL,NULL)==XUI_OK);
 const char* optional[]={NULL,""," ; "," c "};assert(xuiTagInputSetTags(tags,optional,4)==XUI_OK&&xuiTagInputGetTagCount(tags)==1&&!strcmp(xuiTagInputGetTag(tags,0),"c"));
 assert(xuiTagInputSetTags(tags,NULL,0)==XUI_OK&&xuiTagInputGetTagCount(tags)==0);
 assert(xuiTagInputSetMaxTags(tags,2)==XUI_OK&&xuiTagInputSetTags(tags,values,3)==XUI_ERROR_BUFFER_TOO_SMALL&&xuiTagInputGetTagCount(tags)==0);
 assert(xuiTagInputSetMaxTags(tags,0)==XUI_OK);
 assert(xuiTagInputSetTags(tags,values,-1)==XUI_ERROR_INVALID_ARGUMENT&&xuiTagInputSetTags(tags,NULL,1)==XUI_ERROR_INVALID_ARGUMENT);
 const char* original[]={"old"};const char* replacement[]={"one","two","three"};assert(xuiTagInputSetTags(tags,original,1)==XUI_OK);
 unsigned failures=0,successes=0;
 for(int n=0;n<16;++n){fixtureTagPrefixBegin();fixtureTagBudget(n);int status=xuiTagInputSetTags(tags,replacement,3);fixtureTagBudget(-1);
  if(status==XUI_OK){++successes;assert(xuiTagInputGetTagCount(tags)==3&&!strcmp(xuiTagInputGetTag(tags,2),"three"));}
  else{++failures;assert(xuiTagInputGetTagCount(tags)==1&&!strcmp(xuiTagInputGetTag(tags,0),"old"));}
  assert(xuiTagInputSetTags(tags,original,1)==XUI_OK);fixtureTagPrefixEnd();fixtureTagHeapEqual();
 }
 assert(failures&&successes);const char* warm[]={"a","b","c"};assert(xuiTagInputSetTags(tags,warm,3)==XUI_OK);
 fixtureTagHotBegin();fixtureTagNativeHot(tags);fixtureTagHotReport(1);
 xui_widget closing=NULL;assert(xuiTagInputCreate(ui,&closing,NULL)==XUI_OK);
 assert(xuiTagInputSetChange(closing,close_change,NULL)==XUI_OK&&xuiTagInputSetTags(closing,warm,3)==XUI_OK);
 assert(xuiDestroyChecked(ui)==XUI_OK&&xuiFontReleaseChecked(font)==XUI_OK&&xuiProxyReleaseChecked(proxy)==XUI_OK);fixtureTagCleanup();
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(!after.LiveCount&&!after.InvalidFreeCount&&!after.DoubleFreeCount);
 puts("TagBatch native: trim, duplicate preservation, nullable entries, atomic OOM, callbacks and cleanup passed");return 0;
}
