/* Real wrapper/native control transitions. No generated-language API claim. */
#define XRT_MODULE_TASK_POOL
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_ONCE
#define XRT_MODULE_THREAD_KEY
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_WAIT
#ifdef XL6_TASK_POOL_TEST_RUNTIME
#define XRUNTIME_IMPLEMENTATION
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#else
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#endif
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static const XL6_TaskPoolControlV1* api;
static XL6_OwnershipCatalog* catalog;
static xatomic32 hook_armed={0},hook_seen={0},hook_resume={0};
static xatomic64 hook_thread={0};
static void hook_wait(void)
{
    if(xrtAtomic64Load(&hook_thread,XMEMORY_ACQUIRE)!=xrtThreadCurrentId())return;
    uint32 armed=1;
    if(!xrtAtomic32CompareExchange(&hook_armed,&armed,2,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return;
    xrtAtomic32Store(&hook_seen,1,XMEMORY_RELEASE);XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtAtomic32Load(&hook_resume,XMEMORY_ACQUIRE)){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
}
static ptr allocate(ptr context,size_t bytes) {(void)context;hook_wait();return malloc(bytes);}
static ptr resize(ptr context,ptr memory,size_t bytes) {(void)context;hook_wait();return realloc(memory,bytes);}
static void release(ptr context,ptr memory) {(void)context;free(memory);}
static void wait_value(xatomic32* value,uint32 expected)
{
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    while(xrtAtomic32Load(value,XMEMORY_ACQUIRE)<expected){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
}
static void freeze(xrtownershipscope* scope)
{
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
}
static void balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static XL6_TaskPool* create(void)
{XL6_TaskPoolConfig config={1,8,0};XL6_TaskPool* pool=api->Create(&config);assert(pool);return pool;}
static XL6_TaskPoolStatsV1 snapshot(XL6_TaskPool* pool)
{XL6_TaskPoolStatsV1 stats={.size=sizeof(stats)};assert(api->Snapshot(pool,&stats));return stats;}
static void expect_state(void)
{assert(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();}
static void probe_pool(XL6_TaskPool* pool,bool busy)
{
    xrtownershipscope frozen={0};freeze(&frozen);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;xrtownershipref ref=xl6TaskPoolOwnership(pool);
    assert(xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&preparation,&completion)==
        (busy?XL6_OWNERSHIP_ADMISSION_BUSY:XL6_OWNERSHIP_ADMISSION_ADMITTED));
    assert(!completion&&(busy?(!adapter&&!preparation):(adapter&&preparation)));
    assert(xrtOwnershipScopeEnd(&frozen));
}
static void empty_round(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TaskPool* pool=create();
    XL6_TaskPoolStatsV1 stats=snapshot(pool);assert(!stats.initialized&&!stats.closed&&!stats.submitted&&!stats.threads);probe_pool(pool,false);
    XL6_TaskPoolWaitStatusV1 status=XL6_TASK_POOL_READY_V1;
    assert(!api->WaitFor(pool,0,NULL,&status)&&status==XL6_TASK_POOL_TIMEOUT_V1);expect_state();
    assert(api->Close(pool)&&api->Close(pool));
    assert(xrtMemDebugFailAfter(0));
    stats=snapshot(pool);assert(stats.closed&&!stats.initialized);
    assert(api->WaitFor(pool,0,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
    assert(api->WaitFor(pool,INT64_MAX,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(!api->Start(pool));expect_state();
    assert(api->TryQuiesce(pool)==XL6_MODULE_PREPARE_READY&&api->Destroy(pool));balanced(&before);
}
typedef struct Gate {xatomic32 stage,run_resume,drop_resume,self_checks;} Gate;
typedef struct Data {XL6_TaskPool* pool;Gate* gate;bool nested;} Data;
static xtaskoutcome run(xcancel* cancel,ptr raw,xtaskvalue* result)
{
    Data* data=raw;(void)cancel;(void)result;xrtAtomic32Store(&data->gate->stage,1,XMEMORY_RELEASE);
    wait_value(&data->gate->run_resume,1);return XTASK_SUCCESS;
}
static void drop(ptr raw,ptr context)
{
    Data* data=raw;assert(!context);Gate* gate=data->gate;
    if(data->nested){
        XL6_TaskPoolWaitStatusV1 status=XL6_TASK_POOL_READY_V1;
        assert(!api->WaitFor(data->pool,-1,NULL,&status)&&status==XL6_TASK_POOL_TIMEOUT_V1);expect_state();
        (void)xrtAtomic32FetchAdd(&gate->self_checks,1,XMEMORY_RELEASE);
    }
    xrtAtomic32Store(&gate->stage,2,XMEMORY_RELEASE);wait_value(&gate->drop_resume,1);
    xrtFree(data);xrtAtomic32Store(&gate->stage,3,XMEMORY_RELEASE);
}
static void gate_init(Gate* gate,bool released)
{
    xrtAtomic32Init(&gate->stage,0);xrtAtomic32Init(&gate->run_resume,released?1:0);
    xrtAtomic32Init(&gate->drop_resume,released?1:0);xrtAtomic32Init(&gate->self_checks,0);
}
static xfuture* submit(XL6_TaskNativeScopeV1* scope,Gate* gate)
{
    Data* data=xrtMalloc(sizeof(*data));assert(data);*data=(Data){scope->pool,gate,true};
    xtaskargs args={0};args.Destroy=drop;xfuture* future=xrtTaskSubmit(scope->native,run,data,&args);assert(future);return future;
}
static void jobs_round(bool cancel_queued)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TaskPool* pool=create();
    XL6_TaskNativeScopeV1 scope={0};assert(xl6TaskNativeBeginV1(pool,&scope));
    Gate first,second;gate_init(&first,false);gate_init(&second,true);
    xfuture* future=submit(&scope,&first);wait_value(&first.stage,1);
    xfuture* queued=cancel_queued?submit(&scope,&second):NULL;xl6TaskNativeEndV1(&scope);
    assert(api->Close(pool));XL6_TaskPoolWaitStatusV1 status;
    assert(api->WaitFor(pool,0,NULL,&status)&&status==XL6_TASK_POOL_TIMEOUT_V1);
    xcancel* cancel=xrtCancelCreate();assert(cancel&&xrtCancelRequest(cancel));
    assert(api->WaitFor(pool,-1,cancel,&status)&&status==XL6_TASK_POOL_CANCELLED_V1);xrtCancelDestroy(cancel);
    assert(!api->WaitFor(pool,-2,NULL,&status)&&status==XL6_TASK_POOL_TIMEOUT_V1);
    assert(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!api->WaitFor(pool,INT64_MIN,NULL,&status));assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!api->WaitUntil(pool,UINT64_MAX,NULL,&status));assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    if(cancel_queued){
        assert(api->Cancel(pool));wait_value(&second.stage,3);assert(xrtAtomic32Load(&second.self_checks,XMEMORY_ACQUIRE)==1);
        xfutureresult result={0};assert(xrtFutureResult(queued,&result)&&result.State==XFUTURE_CANCELLED);
    }
    XL6_TaskPoolStatsV1 stats=snapshot(pool);assert(stats.initialized&&stats.closed&&stats.submitted==(cancel_queued?2u:1u));
    assert(stats.cancelling==cancel_queued&&stats.threads==1&&stats.queueLimit==8);
    assert(api->TryQuiesce(pool)==XL6_MODULE_PREPARE_BUSY);
    xrtAtomic32Store(&first.run_resume,1,XMEMORY_RELEASE);wait_value(&first.stage,2);
    assert(api->WaitFor(pool,0,NULL,&status)&&status==XL6_TASK_POOL_TIMEOUT_V1);
    assert(api->TryQuiesce(pool)==XL6_MODULE_PREPARE_BUSY);
    xrtAtomic32Store(&first.drop_resume,1,XMEMORY_RELEASE);
    assert(api->WaitFor(pool,5000,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
    assert(xrtAtomic32Load(&first.self_checks,XMEMORY_ACQUIRE)==1);
    stats=snapshot(pool);assert(stats.completed==stats.submitted&&stats.succeeded==1&&stats.cancelled==(cancel_queued?1u:0u));
    assert(api->Quiesce(pool));XL6_TaskPoolStatsV1 retired=snapshot(pool);
    assert(retired.initialized&&retired.completed==stats.completed&&retired.succeeded==stats.succeeded&&retired.closed);
    xrtFutureDestroy(queued);xrtFutureDestroy(future);assert(api->Destroy(pool));balanced(&before);
}
typedef struct Waiter {
    XL6_TaskPool* pool;xcancel* cancel;bool ok,quiesce,inject;
    XL6_TaskPoolWaitStatusV1 status;xatomic32 done;
} Waiter;
static int32 wait_thread(ptr raw)
{
    Waiter* waiter=raw;
    if(waiter->inject)assert(xrtMemDebugFailAfter(0));
    if(waiter->quiesce)waiter->ok=api->Quiesce(waiter->pool);
    else waiter->ok=api->WaitFor(waiter->pool,5000,waiter->cancel,&waiter->status);
    if(waiter->inject){assert(!waiter->ok&&xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtClearError();}
    xrtAtomic32Store(&waiter->done,1,XMEMORY_RELEASE);return 0;
}
static void join(xthread* thread)
{assert(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK);xrtThreadDestroy(thread);}
static void reservation_round(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TaskPool* pool=create();
    XL6_TaskNativeScopeV1 scope={0};assert(xl6TaskNativeBeginV1(pool,&scope)&&api->Close(pool));probe_pool(pool,true);
    XL6_TaskPoolWaitStatusV1 status;
    assert(!api->WaitFor(pool,-1,NULL,&status));expect_state();
    xcancel* cancel=xrtCancelCreate();assert(cancel);
    Waiter fault={.pool=pool,.cancel=cancel,.inject=true};xrtAtomic32Init(&fault.done,0);
    xthread* failing=xrtThreadCreate(wait_thread,&fault,0);assert(failing);join(failing);
    Waiter waiter={.pool=pool,.cancel=cancel};xrtAtomic32Init(&waiter.done,0);
    xthread* waiting=xrtThreadCreate(wait_thread,&waiter,0);assert(waiting);
    /* Observe the actual registration, not an intent flag or a timed sleep. */
    xrtownershipref token=xrtCancelOwnership(cancel);XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){xrtownershipscope frozen={0};freeze(&frozen);size_t refs=0;
        assert(token.Ops->Count(token.Data,&refs));assert(xrtOwnershipScopeEnd(&frozen));
        if(refs>1)break;
        assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    Waiter retiring={.pool=pool,.quiesce=true};xrtAtomic32Init(&retiring.done,0);
    xthread* retire=xrtThreadCreate(wait_thread,&retiring,0);assert(retire);
    for(;;){XL6_TaskPoolStatsV1 stats={.size=sizeof(stats)};
        if(!api->Snapshot(pool,&stats)){expect_state();break;}
        assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    assert(xrtCancelRequest(cancel));join(waiting);assert(waiter.ok&&waiter.status==XL6_TASK_POOL_CANCELLED_V1);
    assert(!xrtAtomic32Load(&retiring.done,XMEMORY_ACQUIRE));
    xl6TaskNativeEndV1(&scope);join(retire);assert(retiring.ok);
    xrtCancelDestroy(cancel);assert(api->Destroy(pool));balanced(&before);
}
typedef struct Creator {XL6_TaskPool* pool;bool ok,native_cancelled;} Creator;
static int32 create_thread(ptr raw)
{
    Creator* creator=raw;xrtAtomic64Store(&hook_thread,xrtThreadCurrentId(),XMEMORY_RELEASE);
    XL6_TaskNativeScopeV1 scope={0};creator->ok=xl6TaskNativeBeginV1(creator->pool,&scope);
    if(creator->ok){
        xtaskpoolstats stats={0};assert(xrtTaskPoolGet(scope.native,&stats));
        creator->native_cancelled=stats.Closed&&stats.Cancelling;xl6TaskNativeEndV1(&scope);
    }
    return 0;
}
static void creation_cancel_round(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TaskPool* pool=create();
    xrtAtomic32Store(&hook_seen,0,XMEMORY_RELEASE);xrtAtomic32Store(&hook_resume,0,XMEMORY_RELEASE);
    xrtAtomic64Store(&hook_thread,0,XMEMORY_RELEASE);xrtAtomic32Store(&hook_armed,1,XMEMORY_RELEASE);
    Creator creator={pool,false,false};xthread* thread=xrtThreadCreate(create_thread,&creator,0);assert(thread);
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtAtomic32Load(&hook_seen,XMEMORY_ACQUIRE)){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    assert(api->Cancel(pool));XL6_TaskPoolStatsV1 stats=snapshot(pool);assert(!stats.initialized&&stats.closed&&stats.cancelling);
    XL6_TaskPoolWaitStatusV1 status;
    assert(xrtMemDebugFailAfter(0));assert(api->WaitFor(pool,0,NULL,&status)&&status==XL6_TASK_POOL_TIMEOUT_V1);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(api->TryQuiesce(pool)==XL6_MODULE_PREPARE_BUSY);
    xrtAtomic32Store(&hook_resume,1,XMEMORY_RELEASE);join(thread);assert(creator.ok&&creator.native_cancelled);
    xrtAtomic32Store(&hook_armed,0,XMEMORY_RELEASE);xrtAtomic64Store(&hook_thread,0,XMEMORY_RELEASE);
    assert(api->WaitFor(pool,5000,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
    stats=snapshot(pool);assert(stats.initialized&&stats.closed&&stats.cancelling&&!stats.submitted);
    assert(api->Quiesce(pool)&&api->Destroy(pool));balanced(&before);
}
static void allocation_prefixes(void)
{
    unsigned failed=0,succeeded=0;
    for(unsigned budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_TaskPool* pool=create();
        assert(xrtMemDebugFailAfter(budget));bool started=api->Start(pool),injected=xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();xrtClearError();
        if(started){assert(!injected);++succeeded;}else{assert(injected);++failed;}
        XL6_TaskPoolStatsV1 stats=snapshot(pool);assert(stats.initialized==started&&!stats.submitted);
        assert(api->Quiesce(pool)&&api->Destroy(pool));balanced(&before);
    }
    assert(failed&&succeeded);
}

typedef struct NativeTail {
    XL6_TaskPool* pool;
    xthreadkey* key;
    xatomic32 entered,resume,finished,returned;
} NativeTail;
static void native_tail_cleanup(ptr raw)
{
    NativeTail* tail=raw;
    xrtAtomic32Store(&tail->entered,1,XMEMORY_RELEASE);
    wait_value(&tail->resume,1);
    xrtAtomic32Store(&tail->finished,1,XMEMORY_RELEASE);
}
static xtaskoutcome native_tail_job(xcancel* cancel,ptr raw,xtaskvalue* result)
{
    NativeTail* tail=raw;(void)cancel;(void)result;
    assert(!xrtThreadKeyGet(tail->key)&&xrtThreadKeySet(tail->key,tail));
    return XTASK_SUCCESS;
}
static int32 native_tail_retire(ptr raw)
{
    NativeTail* tail=raw;XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    do {
        assert(api->TryQuiesce(tail->pool)==XL6_MODULE_PREPARE_BUSY);
        assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();
    }while(!xrtAtomic32Load(&tail->entered,XMEMORY_ACQUIRE));
    /* It must RETURN while the actual native wrapper is still in cleanup.
     * A zero-time job drain followed by blocking Destroy cannot do that. */
    xrtAtomic32Store(&tail->returned,1,XMEMORY_RELEASE);return 0;
}
static void native_tail_round(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    NativeTail tail={.pool=create()};tail.key=xrtThreadKeyCreate(native_tail_cleanup);assert(tail.key);
    XL6_TaskNativeScopeV1 scope={0};assert(xl6TaskNativeBeginV1(tail.pool,&scope));
    xfuture* future=xrtTaskSubmit(scope.native,native_tail_job,&tail,NULL);assert(future);xl6TaskNativeEndV1(&scope);
    assert(xrtFutureWaitFor(future,5000)==XWAIT_OK);xrtFutureDestroy(future);
    XL6_TaskPoolWaitStatusV1 status;assert(api->Close(tail.pool));
    assert(api->WaitFor(tail.pool,5000,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
    assert(!xrtAtomic32Load(&tail.entered,XMEMORY_ACQUIRE)); /* drain is not join */
    xthread* controller=xrtThreadCreate(native_tail_retire,&tail,0);assert(controller);
    wait_value(&tail.entered,1);
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(1000);
    while(!xrtAtomic32Load(&tail.returned,XMEMORY_ACQUIRE)){
        assert(!xl6DeadlineExpiredV1(end)&&"nonblocking retirement must return before native ThreadKey cleanup");xrtThreadYield();
    }
    join(controller);assert(!xrtAtomic32Load(&tail.finished,XMEMORY_ACQUIRE));
    XL6_TaskPoolStatsV1 stats=snapshot(tail.pool);assert(stats.initialized&&stats.submitted==1&&stats.completed==1&&stats.succeeded==1);
    assert(api->TryQuiesce(tail.pool)==XL6_MODULE_PREPARE_BUSY);
    assert(xrtMemDebugFailAfter(0));xrtAtomic32Store(&tail.resume,1,XMEMORY_RELEASE);
    end=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){XL6_ModulePrepareResult state=api->TryQuiesce(tail.pool);if(state==XL6_MODULE_PREPARE_READY)break;
        assert(state==XL6_MODULE_PREPARE_BUSY&&!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xrtAtomic32Load(&tail.finished,XMEMORY_ACQUIRE));stats=snapshot(tail.pool);
    assert(stats.initialized&&stats.submitted==1&&stats.completed==1&&stats.succeeded==1&&stats.closed);
    assert(api->TryQuiesce(tail.pool)==XL6_MODULE_PREPARE_READY&&api->Destroy(tail.pool));
    assert(xrtThreadKeyDestroy(tail.key));balanced(&before);
}
int main(void)
{
    const xallocator allocator={NULL,allocate,resize,release};assert(xrtSetAllocator(&allocator));
    assert(xrtMemDebugEnable(true));api=xl6TaskPoolControlV1Get();assert(api&&api->size==sizeof(*api));
    xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainTaskPoolV1Get()};
    catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
    for(unsigned i=0;i<16;++i){empty_round();jobs_round(false);jobs_round(true);reservation_round();creation_cancel_round();native_tail_round();}
    allocation_prefixes();
    xl6OwnershipCatalogDestroyV1(catalog);
    puts("Task pool control: 96 balanced lifetimes, 32 run/drop barriers, 16 real creating-cancel races, 16 control-retirement handoffs, 16 nonblocking native ThreadKey tails, 32 allocation prefixes, no hidden pool on read");
    return 0;
}
