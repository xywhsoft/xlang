/* The independent native suite and boundary/OOM checks use the actual SDK. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_code_folding.h"
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
#define main original_fold_main
#include "tests/xui/xui_code_fold_state_test.c"
#undef main
#include <assert.h>
#include <stdio.h>
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 assert(xrtMemDebugEnable(true));assert(original_fold_main()==0);
 const xui_code_fold_range_t ranges[2]={{0,0,2,0,0},{0,4,6,0,0}};
 for(int operation=0;operation<3;++operation)for(int allowance=0;allowance<8;++allowance){
  fixtureFoldPrefixBegin();xui_code_fold_state state=NULL;int status=0;
  if(operation==0){fixtureFoldBudget(allowance);status=xuiCodeFoldStateCreate(&state);}
  else{
   assert(xuiCodeFoldStateCreate(&state)==XUI_OK);
   if(operation==1){fixtureFoldBudget(allowance);status=xuiCodeFoldStateSetRanges(state,ranges,2);}
   else{
    int output=37;assert(xuiCodeFoldStateSetRanges(state,ranges,2)==XUI_OK);
    assert(xuiCodeFoldStateFoldAll(state)==XUI_OK);fixtureFoldBudget(allowance);
    status=xuiCodeFoldStateIsLineVisible(state,1,&output);
    assert(status==XUI_OK ? output==0:output==1); /* Failed output must not be published by a binding. */
   }
  }
  assert(status==XUI_OK||status==XUI_ERROR_OUT_OF_MEMORY);fixtureFoldPrefixEnd();
  xuiCodeFoldStateDestroy(state);fixtureFoldHeapEqual();
 }
 xui_code_fold_state state=NULL;assert(xuiCodeFoldStateCreate(&state)==XUI_OK);
 fixtureFoldNativeSet(state);assert(xuiCodeFoldStateFoldAll(state)==XUI_OK);
 int count;assert(xuiCodeFoldStateGetVisibleLineCount(state,7,&count)==XUI_OK&&count==3);
 fixtureFoldHotBegin();fixtureFoldBudget(0);fixtureFoldNativeHot(state);fixtureFoldBudget(-1);fixtureFoldHotReport(1);
 xuiCodeFoldStateDestroy(state);xrtClearError();
 xmemdebugsnapshot end;xrtMemDebugSnapshot(&end);assert(!end.LiveCount&&!end.InvalidFreeCount&&!end.DoubleFreeCount);
 puts("CodeFold native: original suite, allocation prefixes, failed output and cached mapping passed");return 0;
}
