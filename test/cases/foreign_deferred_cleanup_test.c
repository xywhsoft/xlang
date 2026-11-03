#include "src/runtime/xl6_deferred_cleanup.h"
#include <assert.h>
#include <stdio.h>
static unsigned onceCalls,retryCalls,failureCalls;
static bool once(void){++onceCalls;return true;}
static bool failure(void){++failureCalls;return false;}
static XL6_DeferredCleanupV1 retryWork;
static bool retry(void)
{
    if (++retryCalls<3) xl6DeferredCleanupScheduleV1(&retryWork);
    return true;
}
static XL6_DeferredCleanupV1 onceWork={.Run=once};
static XL6_DeferredCleanupV1 failWork={.Run=failure};
static XL6_DeferredCleanupV1 retryWork={.Run=retry};
static xatomic32 producersDone,concurrentCalls;
static bool concurrent(void){(void)xrtAtomic32FetchAdd(&concurrentCalls,1,XMEMORY_RELAXED);return true;}
static XL6_DeferredCleanupV1 concurrentWork[4];
static int32 producer(void* raw)
{
    for(unsigned i=0;i<10000;++i)xl6DeferredCleanupScheduleV1(raw);
    (void)xrtAtomic32FetchAdd(&producersDone,1,XMEMORY_RELEASE);
    return 0;
}
static xl6_error_result boundary(void)
{
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    return xl6ErrorScopeEndAtSafePointV1(&scope);
}
int main(void)
{
    assert(boundary().ok);
    xl6DeferredCleanupScheduleV1(&onceWork);xl6DeferredCleanupScheduleV1(&onceWork);
    xl6DeferredCleanupScheduleV1(&retryWork);xl6DeferredCleanupScheduleV1(&failWork);
    xl6_error_result result=boundary();assert(!result.ok && !result.error);
    assert(onceCalls==1 && retryCalls==1 && failureCalls==1);
    assert(boundary().ok && retryCalls==2);
    assert(boundary().ok && retryCalls==3);
    assert(boundary().ok && retryCalls==3);
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    xerror* primary=xrtErrorCreate(XERR_ARGUMENT,"test",17,"primary");assert(primary);
    xrtSetErrorTake(primary);xl6ErrorScopeCaptureFailure(&scope);
    xl6DeferredCleanupScheduleV1(&failWork);
    result=xl6ErrorScopeEndAtSafePointV1(&scope);
    assert(!result.ok && result.error==primary && failureCalls==2);xrtErrorFree(result.error);
    xerror* prior=xrtErrorCreate(XERR_STATE,"test",1,"previous");assert(prior);xrtSetErrorTake(prior);
    assert(boundary().ok && xrtGetError()==prior);xrtClearError();
    xthread* threads[4]={0};
    for(unsigned i=0;i<4;++i){concurrentWork[i].Run=concurrent;threads[i]=xrtThreadCreate(producer,&concurrentWork[i],0);assert(threads[i]);}
    while(xrtAtomic32Load(&producersDone,XMEMORY_ACQUIRE)<4){assert(boundary().ok);xrtThreadYield();}
    for(unsigned i=0;i<4;++i){assert(xrtThreadWaitFor(threads[i],10000)==XWAIT_OK);xrtThreadDestroy(threads[i]);}
    assert(boundary().ok && xrtAtomic32Load(&concurrentCalls,XMEMORY_RELAXED)>0);
    for(unsigned i=0;i<4;++i)assert(!xrtAtomic32Load(&concurrentWork[i].scheduled,XMEMORY_ACQUIRE));
    puts("deferred cleanup: duplicate coalescing, bounded batches, NULL failure and primary preservation");
    return 0;
}
