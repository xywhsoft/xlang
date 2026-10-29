#define XRT_MODULE_TASK_POOL
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#define XRUNTIME_IMPLEMENTATION
#if defined(XL6_TEST_CORE_HEADER)
#include "lib/xrt/xrt.h"
#else
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#endif
#include <assert.h>
#include <stdio.h>

static void freeze(xrtownershipscope* scope)
{
    double deadline=xrtTimer()+5;
    while(!xrtOwnershipFreezeTryBegin(scope)) {
        assert(xrtTimer()<deadline);xrtThreadYield();
    }
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void startup(unsigned threads)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xrtownershipscope scope={0};freeze(&scope);
    /* Workers cannot enter their first mutation while this freeze is held.
     * Their real native state is initial/unparked, never forged by the test. */
    xtaskpoolconfig config={threads,4,0};xtaskpool* pool=xrtTaskPoolCreate(&config);assert(pool);
    xrtownershipref ref=xrtTaskPoolOwnership(pool);size_t refs=0;
    assert(ref.Ops->Count(ref.Data,&refs) && refs==1);
    const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipadapterv1* adapter=xrtTaskPoolOwnershipAdapterV1(ref,&preparation);assert(adapter && preparation);
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(&ref,1,&ref,1,&graph));
    assert(graph.NodeCount==1 && graph.EdgeCount==1 && graph.ExternalRootCount==0);
    assert(adapter->Hold(pool) && adapter->Claim(pool,&config) && xrtOwnershipScopeEnd(&scope));
    double deadline=xrtTimer()+5;
    for(;;) {
        xrtownershipprepareresult result=preparation->Prepare(pool,&config);
        if(result==XRT_OWNERSHIP_PREPARE_READY)break;
        assert(result==XRT_OWNERSHIP_PREPARE_BUSY && xrtTimer()<deadline);xrtThreadYield();
    }
    freeze(&scope);assert(preparation->Ready(pool) && ref.Ops->Count(pool,&refs) && refs==2);
    adapter->Clear(pool,&config);assert(xrtOwnershipScopeEnd(&scope));
    assert(adapter->Finish(pool,&config) && xrtTaskPoolDestroy(pool));adapter->Drop(pool);
    assert(!xrtGetError());balance(&before);
}
static xatomic32 entered={0},resume={0};
static xtaskoutcome run(xcancel* cancel,ptr data,xtaskvalue* result)
{
    (void)cancel;(void)data;(void)result;xrtAtomic32Store(&entered,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&resume,XMEMORY_ACQUIRE))xrtThreadYield();
    return XTASK_SUCCESS;
}
static void active(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xtaskpoolconfig config={1,4,0};
    xtaskpool* pool=xrtTaskPoolCreate(&config);assert(pool);xfuture* future=xrtTaskSubmit(pool,run,NULL,NULL);assert(future);
    double deadline=xrtTimer()+5;
    while(!xrtAtomic32Load(&entered,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtThreadYield();}
    xrtownershipscope scope={0};freeze(&scope);xrtownershipref ref=xrtTaskPoolOwnership(pool);size_t refs=91;
    const xrtownershippreparationv1* preparation=(const xrtownershippreparationv1*)&config;
    assert(!ref.Ops->Count(pool,&refs) && refs==91);
    assert(!xrtTaskPoolOwnershipAdapterV1(ref,&preparation) && preparation==(const xrtownershippreparationv1*)&config);
    assert(!xrtGetError() && xrtOwnershipScopeEnd(&scope));xrtAtomic32Store(&resume,1,XMEMORY_RELEASE);
    assert(xrtTaskPoolDestroy(pool));xrtFutureDestroy(future);assert(!xrtGetError());balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned round=0;round<8;++round)for(unsigned threads=1;threads<=4;++threads)startup(threads);
    active();puts("task-pool startup: 32 exact frozen graphs, active callback refused, balanced");return 0;
}
