/* Exact resident producer, actual accepted XRT TaskPool jobs and owned cells. */
#define XRT_MODULE_ATOMIC
#define main old_checked_outcome_main
#include "task_checked_outcome_native.c"
#undef main
#include "src/runtime/xl6_task_submit_closure.h"
#include <string.h>

static xatomic32 invokeReady;
static bool entry(XL6_Closure* closure,void* output)
{
    while(!xrtAtomic32Load(&invokeReady,XMEMORY_ACQUIRE))xrtThreadYield();
    return ((bool (*)(void*,int64_t*))closure->invoke)(closure->environment,output);
}
static XL6_Closure* makeClosure(Hooks* hooks)
{
    Environment* environment=xrtMalloc(sizeof(*environment));assert(environment);environment->hooks=hooks;
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(NULL,(void (*)(void))callback,NULL,environment,sizeof(*environment),
        &environmentSchema,&environmentOps,NULL,NULL,0);assert(closure);return closure;
}
static XL6_FutureValueProfileV1 profile(void)
{return (XL6_FutureValueProfileV1){sizeof(XL6_FutureValueProfileV1),XL6_FUTURE_VALUE_TRIVIAL,sizeof(int64_t),1,NULL};}
typedef struct Host {xatomic32 refs,closed,acquired,released;} Host;
static int acquireHost(void* raw)
{
    Host* host=raw;if(xrtAtomic32Load(&host->closed,XMEMORY_ACQUIRE))return 0;
    xrtAtomic32FetchAdd(&host->refs,1,XMEMORY_ACQ_REL);xrtAtomic32FetchAdd(&host->acquired,1,XMEMORY_ACQ_REL);return 1;
}
static void releaseHost(void* raw)
{
    Host* host=raw;assert(xrtAtomic32FetchSub(&host->refs,1,XMEMORY_ACQ_REL)>1);
    xrtAtomic32FetchAdd(&host->released,1,XMEMORY_ACQ_REL);
}
static bool countHost(const void* raw,size_t* count)
{*count=xrtAtomic32Load(&((const Host*)raw)->refs,XMEMORY_ACQUIRE);return *count!=0;}
static bool traceHost(const void* raw,xrtownershipvisitor visit,void* context)
{(void)raw;(void)visit;(void)context;return true;}
static const xrtownershipops hostOps={countHost,traceHost};
static void independentOwners(void)
{
    Balance before={0};snapshot(&before);Hooks hooks={0};Host original={0},adapter={0};
    xrtAtomic32Store(&original.refs,1,XMEMORY_RELEASE);xrtAtomic32Store(&adapter.refs,1,XMEMORY_RELEASE);
    XL6_CodeOwnerGraph owner={{sizeof(owner),&original,acquireHost,releaseHost},UINT64_C(0x584C36434F574E31),&hostOps};
    XL6_CodeOwnerGraph thunk={{sizeof(thunk),&adapter,acquireHost,releaseHost},UINT64_C(0x584C36434F574E31),&hostOps};
    Environment* environment=xrtMalloc(sizeof(*environment));assert(environment);environment->hooks=&hooks;
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(&owner.base,(void (*)(void))callback,NULL,environment,sizeof(*environment),
        &environmentSchema,&environmentOps,NULL,NULL,0);assert(closure);
    const XL6_TaskRuntimeV1* tasks=xl6TaskRuntimeV1Get();XL6_TaskPoolConfig config={1,8,0};
    XL6_TaskPool* pool=tasks->PoolCreate(&config);assert(pool);xrtAtomic32Store(&invokeReady,0,XMEMORY_RELEASE);
    xfuture* future=xl6TaskSubmitClosureV1(pool,profile(),(XL6_CheckedResultClosureBorrowV1){closure,entry,&thunk.base},NULL);assert(future);
    assert(xrtAtomic32Load(&original.refs,XMEMORY_ACQUIRE)==2&&xrtAtomic32Load(&adapter.refs,XMEMORY_ACQUIRE)==2);
    xrtAtomic32Store(&original.closed,1,XMEMORY_RELEASE);xrtAtomic32Store(&adapter.closed,1,XMEMORY_RELEASE);
    xl6ClosureRuntimeV1Get()->Release(closure);xrtAtomic32Store(&invokeReady,1,XMEMORY_RELEASE);
    assert(tasks->PoolQuiesce(pool)&&hooks.runs==1&&hooks.drops==1);
    assert(xrtAtomic32Load(&original.refs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&adapter.refs,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&original.acquired,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&original.released,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&adapter.acquired,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&adapter.released,XMEMORY_ACQUIRE)==1);
    assert(*(const int64_t*)xrtFutureValue(future)==321);assert(tasks->PoolDestroy(pool));xrtFutureDestroy(future);balanced(&before);
}
static void accepted(unsigned body,unsigned cleanup,bool preCancelled)
{
    Balance before={0};snapshot(&before);Hooks hooks={body,cleanup,0,0,0};
    const XL6_TaskRuntimeV1* tasks=xl6TaskRuntimeV1Get();XL6_TaskPoolConfig config={1,8,0};
    XL6_TaskPool* pool=tasks->PoolCreate(&config);assert(pool);XL6_Closure* closure=makeClosure(&hooks);
    xrtAtomic32Store(&invokeReady,0,XMEMORY_RELEASE);
    xcancel* parent=preCancelled?xrtCancelCreate():NULL;if(parent)assert(xrtCancelRequest(parent));
    xfuture* future=xl6TaskSubmitClosureV1(pool,profile(),(XL6_CheckedResultClosureBorrowV1){closure,entry,NULL},parent);assert(future);
    xl6ClosureRuntimeV1Get()->Release(closure);xrtCancelDestroy(parent);
    xrtAtomic32Store(&invokeReady,1,XMEMORY_RELEASE);assert(tasks->PoolQuiesce(pool));
    xfutureresult result={0};assert(xrtFutureResult(future,&result));
    bool cancelled=preCancelled||body==4;
    bool failed=!cancelled&&(body==1||body==2||body==6||body==7||cleanup);
    assert(result.State==(cancelled?XFUTURE_CANCELLED:failed?XFUTURE_FAILED:XFUTURE_RESOLVED));
    assert(hooks.drops==1&&hooks.runs==(preCancelled?0u:1u));
    if(!failed&&!cancelled)assert(result.Value&&*(int64_t*)result.Value==321);
    if(failed){assert(result.Error);
        if(body==1||body==7)assert(xrtErrorCode(result.Error)==901);
        else if(body==2||body==6||cleanup==2)assert(xrtErrorKind(result.Error)==XERR_INTERNAL);
        else assert(xrtErrorCode(result.Error)==902);}
    assert(tasks->PoolDestroy(pool));xrtFutureDestroy(future);balanced(&before);
}
#ifndef XL6_TASK_CHECKED_MINIMAL
static void prepareFailures(void)
{
    unsigned failures=0,acceptedCount=0;
    for(size_t budget=0;budget<72;++budget){
        Balance before={0};snapshot(&before);Hooks hooks={0};const XL6_TaskPoolControlV1* controls=xl6TaskPoolControlV1Get();
        XL6_TaskPoolConfig config={1,8,0};XL6_TaskPool* pool=controls->Create(&config);assert(pool&&controls->Start(pool));
        XL6_Closure* closure=makeClosure(&hooks);assert(xrtMemDebugFailAfter(budget));
        xfuture* future=xl6TaskSubmitClosureV1(pool,profile(),(XL6_CheckedResultClosureBorrowV1){closure,entry,NULL},NULL);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        assert(triggered||future);if(!future)++failures;else ++acceptedCount;
        /* Failure never consumes the borrowed callback. Its real owner remains
         * callable until the caller releases it, including allocation rollback. */
        assert(!hooks.drops);xl6ClosureRuntimeV1Get()->Release(closure);
        assert(controls->Quiesce(pool));assert(hooks.drops==1&&hooks.runs==(future?1u:0u));
        if(future)assert(xrtFutureState(future)==XFUTURE_RESOLVED);
        assert(controls->Destroy(pool));xrtFutureDestroy(future);balanced(&before);
    }
    assert(failures&&acceptedCount);printf("Task submit caller OOM: %u rejected prefixes, %u accepted, all balanced\n",failures,acceptedCount);
}
static XL6_TaskOutcome seedWorker(struct xcancel* cancel,void* raw,XL6_TaskResult* output)
{(void)cancel;(void)raw;(void)output;assert(xrtMemDebugFailAfter(0));return XL6_TASK_SUCCESS;}
static bool shouldNotRun(XL6_Closure* closure,void* output)
{(void)closure;(void)output;assert(!"failed cell allocation must not invoke callback");return false;}
static void workerFailure(void)
{
    Balance before={0};snapshot(&before);Hooks hooks={0};const XL6_TaskRuntimeV1* tasks=xl6TaskRuntimeV1Get();
    XL6_TaskPoolConfig config={1,8,0};XL6_TaskPool* pool=tasks->PoolCreate(&config);assert(pool);
    static const XL6_CaptureLayoutV2 empty={sizeof(empty),1,0,NULL};
    void* seed=xl6TaskRuntimeV3Get()->Allocate(NULL,1,seedWorker,&empty);assert(seed);
    xfuture* seeded=tasks->Submit(pool,seed,NULL,NULL,false);assert(seeded&&xrtFutureWait(seeded)==XWAIT_OK);
    XL6_Closure* closure=makeClosure(&hooks);
    xfuture* future=xl6TaskSubmitClosureV1(pool,profile(),(XL6_CheckedResultClosureBorrowV1){closure,shouldNotRun,NULL},NULL);assert(future);
    xl6ClosureRuntimeV1Get()->Release(closure);assert(tasks->PoolQuiesce(pool));
    assert(xrtFutureState(future)==XFUTURE_FAILED&&hooks.runs==0&&hooks.drops==1);
    assert(tasks->PoolDestroy(pool));xrtFutureDestroy(future);xrtFutureDestroy(seeded);balanced(&before);
}
#endif
int main(void)
{
#ifndef XL6_TASK_CHECKED_MINIMAL
    assert(xrtMemDebugEnable(true));
#endif
    xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    unsigned jobs=0;const unsigned bodies[]={0,1,2,4,6,7};
    for(unsigned repeat=0;repeat<4;++repeat)
        for(size_t body=0;body<sizeof(bodies)/sizeof(*bodies);++body)
            for(unsigned cleanup=0;cleanup<3;++cleanup)
                for(unsigned cancelled=0;cancelled<2;++cancelled){accepted(bodies[body],cleanup,cancelled!=0);++jobs;}
    assert(jobs==144);
    independentOwners();
#ifndef XL6_TASK_CHECKED_MINIMAL
    prepareFailures();workerFailure();
#endif
    puts("Task submit resident: 144 exact owned results/body/cleanup/cancel outcomes, private rollback and balance ok");
    return 0;
}
