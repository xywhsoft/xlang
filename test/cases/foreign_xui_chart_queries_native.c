#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_chart_queries.h"
#include <assert.h>
#include <string.h>
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
extern void fixtureChartNativeOracle(void);
int main(void)
{
#if defined(XGE_TEST_STATIC)
    const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
    assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
    const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
    assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
    assert(xrtMemDebugEnable(true));fixtureChartNativeOracle();
    xmemdebugsnapshot end;xrtMemDebugSnapshot(&end);
    assert(!end.LiveCount && !end.InvalidFreeCount && !end.DoubleFreeCount);
    return 0;
}
