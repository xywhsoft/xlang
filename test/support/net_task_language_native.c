/* Reuse only the existing NETWORK outcome fixture's environment/check helpers.
 * Its renamed entry has no TaskPool symbols or synthetic frame implementation. */
#define main old_net_frame_outcome_main
#include "task_net_frame_outcome.c"
#undef main
#include "src/runtime/xl6_task_net_language.h"
#include "src/runtime/xl6_execution_net_engine.h"
static xatomic32 gate;
static bool invoke(XL6_Closure* closure,void* output)
{
    while(!xrtAtomic32Load(&gate,XMEMORY_ACQUIRE))xrtThreadYield();
    return ((bool (*)(void*,int64_t*))closure->invoke)(closure->environment,output);
}
static XL6_FutureValueProfileV1 profile(void)
{return (XL6_FutureValueProfileV1){sizeof(XL6_FutureValueProfileV1),XL6_FUTURE_VALUE_TRIVIAL,sizeof(int64_t),1,NULL};}
static XL6_Closure* closureFor(Hooks* hooks)
{
    Environment* env=xrtMalloc(sizeof(*env));assert(env);env->hooks=hooks;
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(NULL,(void (*)(void))callback,NULL,env,sizeof(*env),
        &environmentSchema,&environmentOps,NULL,NULL,0);assert(closure);return closure;
}
static XL6_ExecutionResourceV1* createEngine(XL6_ExecutionSourceV1** source)
{
    *source=xl6ExecutionSourceCreateV1();assert(*source);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.NodeCacheBytes=0;
    config.Backend=XNET_PORT_SELECT;
    XL6_ExecutionResourceV1* engine=xl6ExecutionNetEngineCreateV1(*source,&config);assert(engine);
    assert(xl6ExecutionNetEngineStartV1(engine));return engine;
}
static void destroyEngine(XL6_ExecutionSourceV1** source,XL6_ExecutionResourceV1* engine)
{
    assert(xl6ExecutionResourceCloseV1(engine));bool ready=false;
    assert(xl6ExecutionNetEngineWaitV1(engine,10000,&ready)&&ready);
    xl6ExecutionResourceReleaseV1(engine);
    assert(xl6ExecutionSourceCloseV1(*source));
    uint64 until=xrtClock()+UINT64_C(10000000);
    while(*source){XL6_ExecutionRetireResultV1 r=xl6ExecutionSourceDestroyV1(source);assert(r.ok);
        if(!r.ready){assert(xrtClock()<until);xrtThreadYield();}}
}
static void accepted(unsigned body,unsigned cleanup,bool preCancelled,bool delayed)
{
    Balance before={0};snapshot(&before);Hooks hooks={body,cleanup,0,0,0};
    XL6_ExecutionSourceV1* source=NULL;XL6_ExecutionResourceV1* engine=createEngine(&source);
    XL6_Closure* closure=closureFor(&hooks);xcancel* parent=preCancelled?xrtCancelCreate():NULL;
    if(parent)assert(xrtCancelRequest(parent));
    xrtAtomic32Store(&gate,0,XMEMORY_RELEASE);
    XL6_CheckedResultClosureBorrowV1 borrow={closure,invoke,NULL};
    xfuture* future=delayed?xl6NetEngineSubmitUntilCancelV1(profile(),engine,0,borrow,parent,7):
        xl6NetEngineSubmitCancelV1(profile(),engine,borrow,parent,7);assert(future);
    xl6ClosureRuntimeV1Get()->Release(closure);xrtCancelDestroy(parent);
    xrtAtomic32Store(&gate,1,XMEMORY_RELEASE);assert(xrtFutureWait(future)==XWAIT_OK);
    xfutureresult result={0};assert(xrtFutureResult(future,&result));
    bool cancelled=preCancelled||body==4;
    bool failed=!cancelled&&(body==1||body==2||body==6||body==7||cleanup);
    assert(result.State==(cancelled?XFUTURE_CANCELLED:failed?XFUTURE_FAILED:XFUTURE_RESOLVED));
    assert(hooks.drops==1&&hooks.runs==(preCancelled?0u:1u));
    if(!failed&&!cancelled)assert(result.Value&&*(const int64_t*)result.Value==321);
    if(failed){assert(result.Error);
        if(body==1||body==7)assert(xrtErrorCode(result.Error)==901);
        else if(body==2||body==6||cleanup==2)assert(xrtErrorKind(result.Error)==XERR_INTERNAL);
        else assert(xrtErrorCode(result.Error)==902);}
    destroyEngine(&source,engine);xrtFutureDestroy(future);balanced(&before);
}
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
    Environment* env=xrtMalloc(sizeof(*env));assert(env);env->hooks=&hooks;
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(&owner.base,(void (*)(void))callback,NULL,env,sizeof(*env),
        &environmentSchema,&environmentOps,NULL,NULL,0);assert(closure);
    XL6_ExecutionSourceV1* source=NULL;XL6_ExecutionResourceV1* engine=createEngine(&source);
    xrtAtomic32Store(&gate,0,XMEMORY_RELEASE);
    xfuture* future=xl6NetEngineSubmitV1(profile(),engine,(XL6_CheckedResultClosureBorrowV1){closure,invoke,&thunk.base},0);assert(future);
    assert(xrtAtomic32Load(&original.refs,XMEMORY_ACQUIRE)==2&&xrtAtomic32Load(&adapter.refs,XMEMORY_ACQUIRE)==2);
    xrtAtomic32Store(&original.closed,1,XMEMORY_RELEASE);xrtAtomic32Store(&adapter.closed,1,XMEMORY_RELEASE);
    xl6ClosureRuntimeV1Get()->Release(closure);xrtAtomic32Store(&gate,1,XMEMORY_RELEASE);
    assert(xrtFutureWait(future)==XWAIT_OK&&*(const int64_t*)xrtFutureValue(future)==321);
    destroyEngine(&source,engine);xrtFutureDestroy(future);
    assert(hooks.runs==1&&hooks.drops==1);
    assert(xrtAtomic32Load(&original.refs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&adapter.refs,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&original.acquired,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&original.released,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&adapter.acquired,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&adapter.released,XMEMORY_ACQUIRE)==1);
    balanced(&before);
}
#ifndef XL6_TASK_CHECKED_MINIMAL
static void preparationFailures(void)
{
    unsigned refused=0,acceptedCount=0;xrtAtomic32Store(&gate,1,XMEMORY_RELEASE);
    for(size_t budget=0;budget<72;++budget){
        Balance before={0};snapshot(&before);Hooks hooks={0};XL6_ExecutionSourceV1* source=NULL;
        XL6_ExecutionResourceV1* engine=createEngine(&source);XL6_Closure* closure=closureFor(&hooks);
        assert(xrtMemDebugFailAfter(budget));
        xfuture* future=xl6NetEngineSubmitV1(profile(),engine,(XL6_CheckedResultClosureBorrowV1){closure,invoke,NULL},0);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        assert(triggered||future);if(future)++acceptedCount;else ++refused;
        assert(!hooks.drops);xl6ClosureRuntimeV1Get()->Release(closure);
        if(future)assert(xrtFutureWait(future)==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);
        destroyEngine(&source,engine);xrtFutureDestroy(future);assert(hooks.drops==1);balanced(&before);
    }
    assert(refused&&acceptedCount);printf("NetTask prepare: budgets=72 refused=%u accepted=%u; borrowed closure preserved, exact balance\n",refused,acceptedCount);
}
#endif
int main(void)
{
#ifndef XL6_TASK_CHECKED_MINIMAL
    assert(xrtMemDebugEnable(true));
#endif
    xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    const unsigned bodies[]={0,1,2,4,6,7};unsigned count=0;
    for(size_t i=0;i<sizeof(bodies)/sizeof(*bodies);++i)
        for(unsigned cleanup=0;cleanup<3;++cleanup)
            for(unsigned cancelled=0;cancelled<2;++cancelled)
                for(unsigned delayed=0;delayed<2;++delayed){accepted(bodies[i],cleanup,cancelled!=0,delayed!=0);++count;}
    assert(count==72);
    independentOwners();
#ifndef XL6_TASK_CHECKED_MINIMAL
    preparationFailures();
#endif
    puts("NetTask language resident: 72 actual jobs/timers, checked body/cleanup/cancel and private rollback balanced");
    return 0;
}
