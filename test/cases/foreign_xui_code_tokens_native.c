/* The independent native suite and boundary/OOM checks use the actual SDK. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_code_tokens.h"
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
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 assert(xrtMemDebugEnable(true));
 const xui_code_token_t input[2]={{0,3,1,2,0},{4,8,1,2,0}};
 xui_code_token_buffer buffer=NULL;assert(xuiCodeTokenBufferCreate(&buffer)==XUI_OK);
 assert(xuiCodeTokenBufferSet(buffer,input,2,42)==XUI_OK);fixtureTokenCheck(buffer);
 xui_code_token_t output={0};int number=37;
 assert(xuiCodeTokenBufferGetTokens(buffer,42,&output,1,&number)==XUI_OK&&number==2&&output.iEndOffset==3);
 assert(xuiCodeTokenBufferGetTokens(buffer,41,NULL,0,&number)==XUI_ERROR_UNSUPPORTED&&number==0);
 assert(xuiCodeTokenBufferGetTokensInRange(buffer,42,3,5,&output,1,&number)==XUI_OK&&number==1&&output.iStartOffset==4);
 assert(xuiCodeTokenBufferGetTokensInRange(buffer,42,3,3,NULL,0,&number)==XUI_OK&&number==0);
 assert(xuiCodeTokenBufferSetRange(buffer,input,2,UINT32_MAX,0,8)==XUI_OK);
 uint32_t version;int start,end;
 assert(xuiCodeTokenBufferGetRange(buffer,&version,&start,&end)==XUI_OK&&version==UINT32_MAX&&start==0&&end==8);
 assert(xuiCodeTokenBufferSetRange(buffer,input,2,43,4,8)==XUI_ERROR_INVALID_ARGUMENT);
 assert(xuiCodeTokenBufferGetVersion(buffer,&version)==XUI_OK&&version==UINT32_MAX);
 xuiCodeTokenBufferClear(buffer);assert(xuiCodeTokenBufferGetCount(buffer)==0);
 assert(xuiCodeTokenBufferGetRange(buffer,&version,&start,&end)==XUI_OK&&version==0&&start==0&&end==0);
 xuiCodeTokenBufferDestroy(buffer);
 unsigned failed=0,succeeded=0;
 for(int operation=0;operation<3;++operation)for(int allowance=0;allowance<8;++allowance){
  fixtureTokenPrefixBegin();buffer=NULL;int status;
  if(operation==0){fixtureTokenBudget(allowance);status=xuiCodeTokenBufferCreate(&buffer);}
  else{
   assert(xuiCodeTokenBufferCreate(&buffer)==XUI_OK);
   if(operation==1){fixtureTokenBudget(allowance);status=xuiCodeTokenBufferSet(buffer,input,2,42);}
   else{
    assert(xuiCodeTokenBufferSet(buffer,input,2,42)==XUI_OK);
    xui_code_token_t large[17];for(int i=0;i<17;++i)large[i]=(xui_code_token_t){2*i,2*i+1,1,2,0};
    fixtureTokenBudget(allowance);status=xuiCodeTokenBufferSetRange(buffer,large,17,43,0,34);
    if(status==XUI_ERROR_OUT_OF_MEMORY)fixtureTokenCheck(buffer);
   }
  }
  fixtureTokenPrefixEnd();assert(status==XUI_OK||status==XUI_ERROR_OUT_OF_MEMORY);
  if(status==XUI_OK)++succeeded;else ++failed;
  xuiCodeTokenBufferDestroy(buffer);fixtureTokenHeapEqual();
 }
 assert(failed&&succeeded);
 assert(xuiCodeTokenBufferCreate(&buffer)==XUI_OK);assert(xuiCodeTokenBufferSet(buffer,input,2,42)==XUI_OK);
 fixtureTokenBudget(0);fixtureTokenNativeHot(buffer);fixtureTokenBudget(-1);xuiCodeTokenBufferDestroy(buffer);
 xmemdebugsnapshot endSnapshot;xrtMemDebugSnapshot(&endSnapshot);assert(!endSnapshot.LiveCount&&!endSnapshot.InvalidFreeCount&&!endSnapshot.DoubleFreeCount);
 puts("CodeTokens native: actual SDK ranges, payload, failure atomicity, allocation prefixes and warmed queries passed");return 0;
}
