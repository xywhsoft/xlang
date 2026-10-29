#include "include/xruntime_decl.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_task_pool_exit.h"
#include <assert.h>
#include <stdio.h>

static const XL6_TaskPoolControlV1* api;
static void until(xatomic32* flag)
{
    xdeadline deadline=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void joined(xthread* thread)
{assert(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK);xrtThreadDestroy(thread);}
static void balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();assert(xrtTempClear());xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
typedef struct Tail {xthreadkey* key;xatomic32 entered,resume,finished;} Tail;
static void tail_drop(void* raw)
{
    Tail* tail=raw;xrtAtomic32Store(&tail->entered,1,XMEMORY_RELEASE);until(&tail->resume);
    xrtAtomic32Store(&tail->finished,1,XMEMORY_RELEASE);
}
static xtaskoutcome work(xcancel* cancel,void* raw,xtaskvalue* value)
{
    (void)cancel;(void)value;Tail* tail=raw;
    assert(xrtThreadKeySet(tail->key,tail));return XTASK_SUCCESS;
}
static void pending_round(void)
{
    const XL6_TaskPoolConfig config={1,8,0};XL6_TaskPool* pool=api->Create(&config);assert(pool);
    Tail tail={0};tail.key=xrtThreadKeyCreate(tail_drop);assert(tail.key);
    XL6_TaskNativeScopeV1 scope={0};assert(xl6TaskNativeBeginV1(pool,&scope));xtaskpool* native=scope.native;
    xfuture* future=xrtTaskSubmit(native,work,&tail,NULL);assert(future);xl6TaskNativeEndV1(&scope);
    assert(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK);xrtFutureDestroy(future);
    XL6_TaskPoolWaitStatusV1 status;assert(api->Close(pool)&&api->WaitFor(pool,5000,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
    /* Legacy Wait only drains jobs. Preflight with no pending stop must not
     * change that contract or begin physical ThreadKey destruction. */
    assert(!xrtAtomic32Load(&tail.entered,XMEMORY_ACQUIRE));
    assert(xl6TaskPoolModulePrepareV1(pool,false,false)==XL6_MODULE_PREPARE_READY);
    assert(!xrtAtomic32Load(&tail.entered,XMEMORY_ACQUIRE));
    assert(xl6TaskPoolModulePrepareV1(pool,true,false)==XL6_MODULE_PREPARE_BUSY);
    xdeadline deadline=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xrtAtomic32Load(&tail.entered,XMEMORY_ACQUIRE)){
        assert(xl6TaskPoolModulePrepareV1(pool,false,false)==XL6_MODULE_PREPARE_BUSY);
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
    for(unsigned i=0;i<16;++i){
        assert(xl6TaskPoolModulePrepareV1(pool,false,false)==XL6_MODULE_PREPARE_BUSY);
        assert(!xrtAtomic32Load(&tail.finished,XMEMORY_ACQUIRE));
    }
    /* Physical completion must stay excluded from Count/Trace while the
     * actual native wrapper is in its ThreadKey tail. No relaxed admission. */
    xrtownershipscope freeze={0};deadline=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    const xrtownershipquiescencev1* policy=xrtTaskPoolOwnershipQuiescenceV1Get();
    assert(policy->Busy(native));size_t count=SIZE_MAX;
    assert(!xrtTaskPoolOwnership(native).Ops->Count(native,&count)&&count==SIZE_MAX);
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(xrtMemDebugFailAfter(0));xrtAtomic32Store(&tail.resume,1,XMEMORY_RELEASE);
    deadline=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){XL6_ModulePrepareResult result=xl6TaskPoolModulePrepareV1(pool,false,false);
        if(result==XL6_MODULE_PREPARE_READY)break;
        assert(result==XL6_MODULE_PREPARE_BUSY&&!xrtDeadlineExpired(deadline));xrtThreadYield();}
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xrtAtomic32Load(&tail.finished,XMEMORY_ACQUIRE)&&xl6TaskRuntimeV1Get()->PoolIsQuiescent(pool));
    assert(api->Destroy(pool)&&xrtThreadKeyDestroy(tail.key));
}
typedef struct Mutation {xatomic32 entered,resume;} Mutation;
static int32 hold_mutation(void* raw)
{
    Mutation* state=raw;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    xrtAtomic32Store(&state->entered,1,XMEMORY_RELEASE);until(&state->resume);
    assert(xrtOwnershipScopeEnd(&mutation));return 0;
}
static void complete_round(void)
{
    const XL6_TaskPoolConfig config={1,8,0};XL6_ExecutionFailuresV1* failures=xl6ExecutionFailuresCreateV1();assert(failures);
    XL6_TaskPool* pool=xl6TaskPoolCreateManagedV1(&config,failures);assert(pool);
    assert(api->Start(pool)&&api->Close(pool));XL6_TaskPoolWaitStatusV1 status;
    assert(api->WaitFor(pool,5000,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
    Mutation state={0};xthread* other=xrtThreadCreate(hold_mutation,&state,0);assert(other);until(&state.entered);
    xrtownershipscope freeze={0};assert(!xrtOwnershipFreezeTryBegin(&freeze));
    xcancel* cancel=xrtCancelCreate();assert(cancel&&xrtCancelRequest(cancel));
    assert(xrtMemDebugFailAfter(0));
    for(unsigned i=0;i<64;++i){
        assert(api->WaitFor(pool,0,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
        assert(api->WaitFor(pool,0,cancel,&status)&&status==XL6_TASK_POOL_READY_V1);
        assert(api->WaitUntil(pool,0,cancel,&status)&&status==XL6_TASK_POOL_READY_V1);
    }
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtCancelDestroy(cancel);
    xrtAtomic32Store(&state.resume,1,XMEMORY_RELEASE);joined(other);
    assert(xl6ExecutionFailuresCheckV1(failures));
    assert(api->TryQuiesce(pool)==XL6_MODULE_PREPARE_READY&&api->Destroy(pool));
    xl6ExecutionFailuresReleaseV1(failures);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));api=xl6TaskPoolControlV1Get();assert(api&&api->size==sizeof(*api));
    assert(xrtTemp(8)&&xrtTempClear());xrtSetErrorKind(XERR_STATE);xrtClearError();
    assert(xl6TaskPoolModulePrepareV1(NULL,false,false)==XL6_MODULE_PREPARE_READY);
    pending_round();complete_round();assert(xrtTempClear());
    for(unsigned i=0;i<32;++i){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        pending_round();complete_round();balanced(&before);}
    puts("TaskPool completion: 32 pending stops resumed outside graph admission, 6144 stable completed waits under unrelated mutation, strict BUSY Count and exact memory balance");
    return 0;
}
