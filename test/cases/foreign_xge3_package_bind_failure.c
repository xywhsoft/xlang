#include <xge3_runtime.h>
#include <assert.h>
#include <stdio.h>
static int status;
int __wrap_xgeHostXrtBind(const xge_host_xrt_contract_t* contract,xge_host_xrt_resolve_proc resolve,void* user,const char** missing)
{
    (void)contract;(void)resolve;(void)user;*missing="deliberate missing symbol";return status;
}
#if defined(_WIN32)
/* Test-only import-slot substitution, matching the public dllimport ABI. */
int (*__imp_xgeHostXrtBind)(const xge_host_xrt_contract_t*,xge_host_xrt_resolve_proc,void*,const char**)=__wrap_xgeHostXrtBind;
#endif
int main(void)
{
    for(status=XGE_HOST_XRT_INVALID_CONTRACT;status>=XGE_HOST_XRT_BUSY;--status){
        assert(xl6Xge3RuntimeBindV1()==status);
        assert(xrtGetError() && xrtErrorCode(xrtGetError())==status);
        xrtClearError();
    }
    status=XGE_HOST_XRT_INVALID_CONTRACT;
    assert(xrtMemDebugFailAfter(0));assert(xl6Xge3RuntimeBindV1()==status);
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtClearError();
    puts("bootstrap reports four checked failures and OOM without abort");
    return 0;
}
