/* Include after a selected XRT implementation plus the selective resolver.
 * This checks the actual table, all exact addresses, and a bounded cold-path
 * workload. It does not execute a UI or an engine callback. */
#include <assert.h>
#include <stdio.h>
#include <time.h>
int main(void)
{
    const size_t count=sizeof(xl6HostXrtSymbolsV1)/sizeof(*xl6HostXrtSymbolsV1);
    assert(count && !xl6HostXrtResolveV1(NULL) && !xl6HostXrtResolveV1("not_an_xrt_function"));
    for(size_t i=0;i<count;++i){
        assert(!i || strcmp(xl6HostXrtSymbolsV1[i-1].name,xl6HostXrtSymbolsV1[i].name)<0);
        assert(xl6HostXrtResolveV1(xl6HostXrtSymbolsV1[i].name)==xl6HostXrtSymbolsV1[i].address);
    }
    assert(xl6HostXrtResolveV1("xrtVersion")== (XL6_HostXrtFunctionV1)xrtVersion);
#if defined(XL6_XGE3_RESOLVER_IMPORTS)
#define XGE_HOST_XRT_SLOT(Name) assert(xl6HostXrtResolveV1(#Name)==(XL6_HostXrtFunctionV1)(Name));
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
#endif
    /* Warm once, then verify that lookup does not consume an error or allocate. */
    xerror* original=xrtErrorCreate(XERR_STATE,"resolver-test",1,"preserve this error");assert(original);
    xrtSetErrorTake(original);
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    assert(xrtMemDebugFailAfter(0));
#endif
    clock_t start=clock();
    volatile XL6_HostXrtFunctionV1 result=NULL;
    for(unsigned round=0;round<4096;++round)
        for(size_t i=0;i<count;++i)result=xl6HostXrtResolveV1(xl6HostXrtSymbolsV1[i].name);
    double elapsed=(double)(clock()-start)/CLOCKS_PER_SEC;
    assert(result && xrtGetError()==original);
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
#endif
    xrtClearError();
    printf("{\"symbols\":%zu,\"tableBytes\":%zu,\"lookupCount\":%zu,\"seconds\":%.6f,\"mean177LookupMicroseconds\":%.3f}\n",
        count,sizeof(xl6HostXrtSymbolsV1),count*(size_t)4096,elapsed,elapsed*1e6/(count*4096.0)*177);
    return 0;
}
