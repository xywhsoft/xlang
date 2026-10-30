#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_concurrent_deadline.h"
#include <assert.h>
#include <stdio.h>
typedef struct Test {
    XL6_TaskPool* pool;xthreadkey* key;
    xatomic32 entered,release,done;XL6_ModulePrepareResult result;
} Test;
static void wait_flag(xatomic32* flag)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtAtomic32Load(flag,XMEMORY_ACQUIRE)){assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
}
static void key_drop(void* raw)
{
    Test* t=raw;xrtAtomic32Store(&t->entered,1,XMEMORY_RELEASE);wait_flag(&t->release);
    assert(!xrtGetError());
    t->result=xl6TaskPoolTryQuiesceV1(t->pool);
    assert(!xrtGetError());xrtAtomic32Store(&t->done,1,XMEMORY_RELEASE);
}
static xtaskoutcome job(xcancel* cancel,void* raw,xtaskvalue* result)
{
    (void)cancel;(void)result;Test* t=raw;assert(xrtThreadKeySet(t->key,t));return XTASK_SUCCESS;
}
int main(void)
{
    for(unsigned round=0;round<30;++round){
        Test t={0};const XL6_TaskRuntimeV1* runtime=xl6TaskRuntimeV1Get();
        XL6_TaskPoolConfig config={1,8,0};t.pool=runtime->PoolCreate(&config);assert(t.pool);
        t.key=xrtThreadKeyCreate(key_drop);assert(t.key);
        XL6_TaskNativeScopeV1 scope={0};assert(xl6TaskNativeBeginV1(t.pool,&scope));
        xfuture* future=xrtTaskSubmit(scope.native,job,&t,NULL);assert(future);
        xl6TaskNativeEndV1(&scope);assert(xrtFutureWaitFor(future,5000)==XWAIT_OK);xrtFutureDestroy(future);
        XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
        while(!xrtAtomic32Load(&t.entered,XMEMORY_ACQUIRE)){
            assert(xl6TaskPoolTryQuiesceV1(t.pool)==XL6_MODULE_PREPARE_BUSY);
            assert(!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
        }
        /* The outer control credit has ended. The native key destructor is
         * outside language activity and is now the ONLY quiesce caller. */
        xrtAtomic32Store(&t.release,1,XMEMORY_RELEASE);wait_flag(&t.done);
        assert(t.result==XL6_MODULE_PREPARE_BUSY);
        deadline=xl6DeadlineAfterMillisecondsV1(5000);
        while(xl6TaskPoolTryQuiesceV1(t.pool)!=XL6_MODULE_PREPARE_READY){
            assert(!xrtGetError()&&!xl6DeadlineExpiredV1(deadline));xrtThreadYield();
        }
        assert(runtime->PoolDestroy(t.pool));assert(xrtThreadKeyDestroy(t.key));
    }
    puts("Task-pool retirement: 30 forced native ThreadKey self-polls return BUSY without error; external physical joins complete");return 0;
}
