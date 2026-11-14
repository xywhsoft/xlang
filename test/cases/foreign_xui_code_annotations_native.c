/* The independent native suite and boundary/OOM checks use the actual SDK. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_code_annotations.h"
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
#define main original_indicator_main
#include "tests/xui/xui_code_search_indicator_test.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <limits.h>
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 assert(xrtMemDebugEnable(true));assert(!original_indicator_main());
 unsigned failed=0,succeeded=0;
 for(int operation=0;operation<3;++operation)for(int allowance=0;allowance<32;++allowance){
  fixtureAnnPrefixBegin();xui_code_annotation_store store=NULL;int status;
  if(operation==0){fixtureAnnBudget(allowance);status=xuiCodeAnnotationStoreCreate(&store);}
  else{
   assert(xuiCodeAnnotationStoreCreate(&store)==XUI_OK);
   if(operation==1){
    assert(xuiCodeAnnotationSetMarker(store,1,4,7,"tip",UINT64_C(4294967296))==XUI_OK);
    fixtureAnnBudget(allowance);status=xuiCodeAnnotationSetMarker(store,1,4,7,"new",UINT64_C(4294967296));
    fixtureAnnPrefixEnd();if(status==XUI_ERROR_OUT_OF_MEMORY)fixtureAnnCheck(store,6);
   }else{
    fixtureAnnNativeSet(store);
    const xui_code_diagnostic_t modified[2]={{0,{0,3},1,NULL,"new","test",UINT64_C(4294967296)},{0,{4,8},2,"W","",NULL,2}};
    fixtureAnnBudget(allowance);status=xuiCodeAnnotationSetDiagnostics(store,modified,2);
    fixtureAnnPrefixEnd();if(status==XUI_ERROR_OUT_OF_MEMORY)fixtureAnnCheck(store,0);
   }
  }
  fixtureAnnPrefixEnd();assert(status==XUI_OK||status==XUI_ERROR_OUT_OF_MEMORY);
  if(status==XUI_OK)++succeeded;else ++failed;
  xuiCodeAnnotationStoreDestroy(store);fixtureAnnHeapEqual();
 }
 assert(failed&&succeeded);
 xui_code_annotation_store store=NULL;assert(xuiCodeAnnotationStoreCreate(&store)==XUI_OK);
 fixtureAnnNativeSet(store);uint32_t version=xuiCodeAnnotationGetDiagnosticVersion(store);
 assert(xuiCodeAnnotationSetMarker(store,INT_MAX-1,4,0,NULL,0)==XUI_OK);
 assert(xuiCodeAnnotationTrackEdit(store,0,0,1,0,0,INT_MAX)==XUI_ERROR_LIMIT_EXCEEDED);
 assert(xuiCodeAnnotationGetDiagnosticVersion(store)==version);fixtureAnnCheck(store,0);
 fixtureAnnBudget(0);fixtureAnnNativeHot(store,version);fixtureAnnBudget(-1);
 xuiCodeAnnotationStoreDestroy(store);
 xmemdebugsnapshot end;xrtMemDebugSnapshot(&end);assert(!end.LiveCount&&!end.InvalidFreeCount&&!end.DoubleFreeCount);
 puts("CodeAnnotations native: actual SDK diagnostics, indicators, atomic failure, allocation prefixes and warmed queries passed");return 0;
}
