#include "release/include/xl_clock.h"
#include "release/include/xrt_decl.h"
#include "src/runtime/xl6_thread.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

enum { THREAD_ID=401 };
static _Atomic(unsigned) started, proceed, ended, callbacks;
static _Atomic(unsigned) captureEntered,captureProceed,releaseReturned,waitEntered;
static _Atomic(bool) captureWindow;
extern xwaitresult __real_xrtThreadWait(xthread*);
xwaitresult __wrap_xrtThreadWait(xthread* thread)
{
    if (captureWindow) atomic_store_explicit(&waitEntered,1,memory_order_release);
    return __real_xrtThreadWait(thread);
}
static xvalue* transferred;
static void await(_Atomic(unsigned)* flag)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(30000);
    while (!atomic_load_explicit(flag,memory_order_acquire)) {
        assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
    }
}
static void reset(void)
{ atomic_store(&started,0); atomic_store(&proceed,0); atomic_store(&ended,0); }
static bool constant(void* environment,int64_t* result)
{ (void)environment; atomic_fetch_add(&callbacks,1); *result=INT64_C(0xFFFFFFFF); return true; }
static bool blocked(void* environment,int64_t* result)
{
    if (environment) { int64 value=0; assert(xrtValueGetInt(*(xvalue**)environment,&value) && value==42); }
    atomic_store_explicit(&started,1,memory_order_release); await(&proceed);
    *result=17; atomic_store_explicit(&ended,1,memory_order_release); return true;
}
static bool stopping(void* environment,int64_t* result)
{
    (void)environment; atomic_store_explicit(&started,1,memory_order_release);
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(30000);
    while (!xrtThreadStopping()) { assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield(); }
    *result=23; return true;
}
static bool selfDrop(void* environment,int64_t* result)
{
    (void)environment; atomic_store_explicit(&started,1,memory_order_release); await(&proceed);
    xvalue* owned=transferred; transferred=NULL; xrtValueRelease(owned);
    *result=31; atomic_store_explicit(&ended,1,memory_order_release); return true;
}
static bool failure(void* environment,int64_t* result)
{ (void)environment; *result=99; return false; }
static bool failureDiagnostic(void* environment,int64_t* result)
{ (void)environment; *result=99; xrtSetErrorKind(XERR_ARGUMENT); return false; }
static void captureDrop(void* raw,void* context)
{ (void)raw; (void)context; xrtSetErrorKind(XERR_STATE); }
static bool captureInvoke(void* environment,int64_t* result)
{
    assert(environment && *(xvalue**)environment);
    atomic_store_explicit(&started,1,memory_order_release); await(&proceed); *result=71; return true;
}
static XL6_Closure* make(bool (*invoke)(void*,int64_t*))
{
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->Create(NULL,(void (*)(void))invoke,
        NULL,NULL,NULL,NULL,NULL,0); assert(closure); return closure;
}
static xvalue* launch(bool (*invoke)(void*,int64_t*))
{
    XL6_Closure* closure=make(invoke); xvalue* thread=xl6ThreadCreateV1(closure,THREAD_ID);
    xl6ClosureRuntimeV1Get()->Release(closure); assert(thread); return thread;
}
static bool collect(xrtownershipref ref,void* context)
{ *(xrtownershipref*)context=ref; return true; }
static xrtownershipref ownerRef(xvalue* thread)
{
    xrtownershipref owner={0}, shell=xrtValueOwnership(thread);
    assert(shell.Ops->Trace(shell.Data,collect,&owner) && owner.Data); return owner;
}
static void fakeDrop(void* raw,void* context) { (void)raw; (void)context; }
static void vectors(void)
{
    XL6_Closure* closure=make(constant);
    assert(!xl6ThreadCreateV1(closure,0)); xrtClearError();
    assert(!xl6ThreadCreateV1(NULL,THREAD_ID)); xrtClearError();
    XL6_Closure* legacy=xl6ClosureRuntimeV1Get()->Create(NULL,(void (*)(void))constant,
        NULL,NULL,NULL,NULL,NULL,0,NULL); assert(legacy);
    assert(!xl6ThreadCreateV1(legacy,THREAD_ID)); xrtClearError();
    xl6ClosureRuntimeV1Get()->Release(legacy);
    xvalue* thread=xl6ThreadCreateV1(closure,THREAD_ID); assert(thread);
    xl6ClosureRuntimeV1Get()->Release(closure);
    xvalue* alias=xrtValueClone(thread); assert(alias);
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    assert(xl6ThreadAdmitsV1(alias,THREAD_ID) && !xl6ThreadAdmitsV1(alias,THREAD_ID+1));
    assert(!xl6ThreadWaitV1(NULL,THREAD_ID) && !xl6ThreadWaitV1(thread,THREAD_ID+1));
    assert(xl6ThreadWaitTimeoutV1(alias,THREAD_ID,INT64_MAX));
    assert(xl6ThreadWaitV1(thread,THREAD_ID) && xl6ThreadExitCodeV1(thread,THREAD_ID)==-1);
    assert(xl6ThreadStateV1(alias,THREAD_ID)==XTHREAD_FINISHED && xrtGetError()==prior);
    assert(xrtMemDebugFailAfter(0)); assert(xl6ThreadAdmitsV1(thread,THREAD_ID));
    assert(!xrtMemDebugFailTriggered() && xrtGetError()==prior); xrtMemDebugFailClear();
    xrtValueRelease(alias); xrtValueRelease(thread); assert(xrtGetError()==prior); xrtClearError();
    static const xvaluehandleops fakeOps={NULL,fakeDrop,NULL,NULL}; void* invalid=(void*)(uintptr_t)1;
    xvalue* foreign=xrtValueHandleTake(&invalid,&fakeOps,NULL); assert(foreign);
    assert(xrtValueTypeIdBind(foreign,THREAD_ID));
    assert(!xl6ThreadAdmitsV1(foreign,THREAD_ID));
    assert(!xl6ThreadOwnershipAdapterV1(xrtValueOwnership(foreign))); xrtValueRelease(foreign);
}
static void graph(void)
{
    reset(); xvalue* captured=xrtValueInt(42); assert(captured);
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->CreateValue(NULL,(void (*)(void))blocked,captured);
    assert(closure); xrtValueRelease(captured);
    xvalue* thread=xl6ThreadCreateV1(closure,THREAD_ID); assert(thread);
    xl6ClosureRuntimeV1Get()->Release(closure); await(&started);
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref owner=ownerRef(thread), edge={0}, child={0}; size_t refs=0;
    const xrtownershipadapterv1* adapter=xl6ThreadOwnershipAdapterV1(owner); assert(adapter);
    assert(owner.Ops->Count(owner.Data,&refs) && refs==2);
    assert(owner.Ops->Trace(owner.Data,collect,&edge) && edge.Data==closure);
    assert(edge.Ops->Count(edge.Data,&refs) && refs==2);
    assert(edge.Ops->Trace(edge.Data,collect,&child) && child.Data);
    assert(child.Ops->Count(child.Data,&refs) && refs==1);
    int token; assert(!adapter->Claim(owner.Data,&token)); assert(xrtOwnershipScopeEnd(&freeze));
    assert(!xl6ThreadWaitTimeoutV1(thread,THREAD_ID,0));
    atomic_store_explicit(&proceed,1,memory_order_release); assert(xl6ThreadWaitV1(thread,THREAD_ID));
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(owner.Ops->Count(owner.Data,&refs) && refs==1);
    edge=(xrtownershipref){0}; assert(owner.Ops->Trace(owner.Data,collect,&edge) && !edge.Data);
    assert(adapter->Hold(owner.Data) && adapter->Claim(owner.Data,&token));
    adapter->Restore(owner.Data,&token); assert(adapter->Claim(owner.Data,&token));
    adapter->Clear(owner.Data,&token); assert(xrtOwnershipScopeEnd(&freeze));
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    assert(!xl6ThreadAdmitsV1(thread,THREAD_ID) && !xl6ThreadWaitV1(thread,THREAD_ID));
    assert(adapter->Finish(owner.Data,&token) && adapter->Finish(owner.Data,&token));
    xrtValueRelease(thread); adapter->Drop(owner.Data); assert(xrtGetError()==prior); xrtClearError();
}
static void stopVector(void)
{
    reset(); xvalue* thread=launch(stopping); await(&started);
    assert(!xl6ThreadWaitTimeoutV1(thread,THREAD_ID,0) && xl6ThreadStopV1(thread,THREAD_ID));
    assert(xl6ThreadWaitTimeoutV1(thread,THREAD_ID,-1));
    assert(xl6ThreadExitCodeV1(thread,THREAD_ID)==23); xrtValueRelease(thread);
}
static int32 freezeHelper(void* raw)
{
    const xvalue* weak=raw; XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(30000);
    while (!xrtValueWeakRefExpired(weak)) { assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield(); }
    xrtownershipscope freeze={0};
    while (!xrtOwnershipFreezeTryBegin(&freeze)) { assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield(); }
    assert(xrtOwnershipScopeEnd(&freeze));
    atomic_store_explicit(&proceed,1,memory_order_release); return 0;
}
static void dropFreeze(void)
{
    reset(); xvalue* thread=launch(blocked); await(&started);
    xvalue* weak=xrtValueWeakRef(thread); assert(weak);
    xthread* helper=xrtThreadCreate(freezeHelper,weak,0); assert(helper);
    xrtValueRelease(thread); assert(atomic_load_explicit(&ended,memory_order_acquire));
    assert(xrtThreadWaitFor(helper,UINT64_C(30000000))==XWAIT_OK);
    xrtThreadDestroy(helper); xrtValueRelease(weak);
}
typedef struct MethodWork { const xvalue* thread; bool result; } MethodWork;
static int32 methodWait(void* raw)
{
    MethodWork* work=raw; work->result=xl6ThreadWaitV1(work->thread,THREAD_ID);
    assert(!xrtGetError()); return 0;
}
static void activeRelease(void)
{
    reset(); xvalue* thread=launch(blocked); await(&started);
    xvalue* weak=xrtValueWeakRef(thread); assert(weak);
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref owner=ownerRef(thread); assert(xrtOwnershipScopeEnd(&freeze));
    MethodWork work={thread,false}; xthread* helper=xrtThreadCreate(methodWait,&work,0); assert(helper);
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(30000); bool busy=false;
    while (!busy) {
        if (xrtOwnershipFreezeTryBegin(&freeze)) {
            size_t refs=0; busy=!owner.Ops->Count(owner.Data,&refs);
            if (busy) assert(!xl6ThreadOwnershipAdapterV1(owner));
            assert(xrtOwnershipScopeEnd(&freeze));
        }
        assert(!xl6DeadlineExpiredV1(deadline)); if (!busy) xrtThreadYield();
    }
    xrtValueRelease(thread); assert(xrtValueWeakRefExpired(weak));
    atomic_store_explicit(&proceed,1,memory_order_release);
    assert(xrtThreadWaitFor(helper,UINT64_C(30000000))==XWAIT_OK && work.result);
    xrtThreadDestroy(helper); xrtValueRelease(weak);
}
static void concurrentMethods(void)
{
    reset(); xvalue* thread=launch(blocked); await(&started);
    MethodWork work[8]; xthread* helpers[8];
    for (unsigned i=0; i<8; ++i) {
        work[i]=(MethodWork){thread,false}; helpers[i]=xrtThreadCreate(methodWait,&work[i],0); assert(helpers[i]);
    }
    atomic_store_explicit(&proceed,1,memory_order_release);
    for (unsigned i=0; i<8; ++i) {
        assert(xrtThreadWaitFor(helpers[i],UINT64_C(30000000))==XWAIT_OK && work[i].result);
        xrtThreadDestroy(helpers[i]);
    }
    assert(xl6ThreadExitCodeV1(thread,THREAD_ID)==17); xrtValueRelease(thread);
}
typedef struct LeaseOwner { _Atomic(unsigned) pins,closed,released; } LeaseOwner;
static int leaseAcquire(void* raw)
{
    LeaseOwner* owner=raw;
    if (atomic_load_explicit(&owner->closed,memory_order_acquire)) return 0;
    atomic_fetch_add(&owner->pins,1); return 1;
}
static void leaseRelease(void* raw)
{
    LeaseOwner* owner=raw; assert(atomic_fetch_sub(&owner->pins,1)==1);
    /* A real host hook, not a replacement for any closure/runtime provider.
       The test owner stays externally resident; it is NOT graph-certified. */
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xrtOwnershipScopeEnd(&freeze)); atomic_fetch_add(&owner->released,1);
}
static void leaseVector(void)
{
    reset(); LeaseOwner state={0}; XL6_CodeOwner owner={sizeof(owner),&state,leaseAcquire,leaseRelease};
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->Create(&owner,(void (*)(void))blocked,
        NULL,NULL,NULL,NULL,NULL,0); assert(closure && atomic_load(&state.pins)==1);
    xvalue* thread=xl6ThreadCreateV1(closure,THREAD_ID); assert(thread);
    xl6ClosureRuntimeV1Get()->Release(closure); atomic_store_explicit(&state.closed,1,memory_order_release);
    await(&started); xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6CodeOwnerScopeActive(&owner) && atomic_load(&state.pins)==1 && !atomic_load(&state.released));
    xrtownershipref lease=xl6CodeLeaseOwnership(closure->lease); size_t refs=0;
    /* This four-field host owner deliberately provides no graph certificate.
       The real scope is active, but graph inspection must still refuse it. */
    assert(!lease.Ops->Count(lease.Data,&refs)); assert(xrtOwnershipScopeEnd(&freeze));
    atomic_store_explicit(&proceed,1,memory_order_release);
    assert(xl6ThreadWaitV1(thread,THREAD_ID) && atomic_load(&state.pins)==0 && atomic_load(&state.released)==1);
    assert(xrtOwnershipFreezeTryBegin(&freeze)); assert(!xl6CodeOwnerScopeActive(&owner));
    assert(xrtOwnershipScopeEnd(&freeze)); xrtValueRelease(thread);
}
static void heapQuiescence(const xmemdebugsnapshot* before)
{
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(30000); xmemdebugsnapshot after;
    do {
        xrtMemDebugSnapshot(&after);
        if (before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes) return;
        assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
    } while (true);
}
static void blockedCaptureDrop(void* raw,void* context)
{
    assert(raw==(void*)(uintptr_t)1 && !context);
    atomic_store_explicit(&captureEntered,1,memory_order_release);
    await(&captureProceed);
}
static bool captureLeaf(const xvalue* value,xrtownershipvisitor visit,void* context)
{ (void)value; (void)context; return visit!=NULL; }
static int32 captureShellRelease(void* raw)
{
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    xrtValueRelease(raw);
    assert(xrtGetError()==prior); xrtClearError();
    atomic_store_explicit(&releaseReturned,1,memory_order_release); return 0;
}
static void captureRetirement(void)
{
    reset(); captureWindow=false;
    atomic_store(&captureEntered,0); atomic_store(&captureProceed,0);
    atomic_store(&releaseReturned,0); atomic_store(&waitEntered,0);
    static const xvaluehandleops ops={NULL,blockedCaptureDrop,NULL,NULL};
    void* raw=(void*)(uintptr_t)1; xvalue* captured=xrtValueHandleTake(&raw,&ops,NULL); assert(captured && !raw);
    assert(xrtValueHandleOwnershipBindPhased(captured,captureLeaf));
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->CreateValue(NULL,(void (*)(void))captureInvoke,captured); assert(closure);
    xrtValueRelease(captured); xvalue* thread=xl6ThreadCreateV1(closure,THREAD_ID); assert(thread);
    xl6ClosureRuntimeV1Get()->Release(closure); await(&started);
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref owner=ownerRef(thread); assert(xrtOwnershipScopeEnd(&freeze));
    atomic_store_explicit(&proceed,1,memory_order_release); await(&captureEntered);
    captureWindow=true; xthread* releaser=xrtThreadCreate(captureShellRelease,thread,0); assert(releaser);
    XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(30000);
    while (!atomic_load_explicit(&waitEntered,memory_order_acquire) &&
        !atomic_load_explicit(&releaseReturned,memory_order_acquire)) {
        assert(!xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
    }
    assert(!atomic_load(&releaseReturned) && atomic_load(&waitEntered));
    /* The wait must not hold a mutation or metadata gate. The real worker
     * stays externally rooted while its already-detached captures retire. */
    assert(xrtOwnershipFreezeTryBegin(&freeze)); size_t refs=0; int token;
    const xrtownershipadapterv1* adapter=xl6ThreadOwnershipAdapterV1(owner); assert(adapter);
    assert(owner.Ops->Count(owner.Data,&refs) && refs==2 && !adapter->Claim(owner.Data,&token));
    assert(xrtOwnershipScopeEnd(&freeze));
    atomic_store_explicit(&captureProceed,1,memory_order_release);
    assert(xrtThreadWaitFor(releaser,UINT64_C(30000000))==XWAIT_OK && atomic_load(&releaseReturned));
    captureWindow=false; xrtThreadDestroy(releaser);
}
static void selfDropVector(void)
{
    reset(); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    transferred=launch(selfDrop); await(&started);
    atomic_store_explicit(&proceed,1,memory_order_release); await(&ended); heapQuiescence(&before);
}
static void errors(void)
{
    xvalue* thread=launch(failure); assert(xl6ThreadWaitV1(thread,THREAD_ID));
    assert(xl6ThreadExitCodeV1(thread,THREAD_ID)==1); xrtValueRelease(thread);
    thread=launch(failureDiagnostic); assert(xl6ThreadWaitV1(thread,THREAD_ID));
    assert(xl6ThreadExitCodeV1(thread,THREAD_ID)==1); xrtValueRelease(thread);
    static const xvaluehandleops errorOps={NULL,captureDrop,NULL,NULL}; void* data=(void*)(uintptr_t)1;
    xvalue* source=xrtValueHandleTake(&data,&errorOps,NULL); assert(source);
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->CreateValue(NULL,(void (*)(void))captureInvoke,source);
    assert(closure); xrtValueRelease(source);
    reset(); thread=xl6ThreadCreateV1(closure,THREAD_ID); assert(thread);
    xl6ClosureRuntimeV1Get()->Release(closure); await(&started);
    atomic_store_explicit(&proceed,1,memory_order_release);
    assert(xl6ThreadWaitV1(thread,THREAD_ID) && xl6ThreadExitCodeV1(thread,THREAD_ID)==1);
    assert(!xrtGetError()); xrtValueRelease(thread);
}
static void failures(void)
{
    XL6_Closure* closure=make(constant); size_t point;
    for (point=0; point<64; ++point) {
        unsigned calls=atomic_load(&callbacks); assert(xrtMemDebugFailAfter(point));
        xvalue* thread=xl6ThreadCreateV1(closure,THREAD_ID);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (hit) assert(!thread && atomic_load(&callbacks)==calls);
        else { assert(thread && xl6ThreadWaitV1(thread,THREAD_ID)); assert(atomic_load(&callbacks)==calls+1); }
        xrtClearError(); xrtValueRelease(thread); if (!hit) break;
    }
    assert(point>0 && point<64); xl6ClosureRuntimeV1Get()->Release(closure);
    printf("Thread constructor: complete caller-thread allocation prefix %zu, no callbacks on rollback\n",point);
}
static void stress(void)
{
    for (unsigned batch=0; batch<32; ++batch) {
        xvalue* threads[8];
        for (unsigned i=0; i<8; ++i) threads[i]=launch(constant);
        for (unsigned i=0; i<8; ++i) {
            assert(xl6ThreadWaitV1(threads[i],THREAD_ID));
            assert(xl6ThreadExitCodeV1(threads[i],THREAD_ID)==-1); xrtValueRelease(threads[i]);
        }
    }
}
int main(int argc,char** argv)
{
    assert(argc==2); assert(xrtMemDebugEnable(true));
    /* Warm actual thread/error/freeze TLS before the measured scope; no
       synthetic roots or relaxed leak baseline are introduced by the host. */
    xvalue* warm=launch(constant); assert(xl6ThreadWaitV1(warm,THREAD_ID)); xrtValueRelease(warm);
    assert(xrtMemDebugFailAfter(0)); xrtMemDebugFailClear();
    xl6ProductMemoryBegin(SIZE_MAX);
    if (!strcmp(argv[1],"vectors")) vectors();
    else if (!strcmp(argv[1],"graph")) graph();
    else if (!strcmp(argv[1],"stop")) stopVector();
    else if (!strcmp(argv[1],"drop-freeze")) dropFreeze();
    else if (!strcmp(argv[1],"active-release")) activeRelease();
    else if (!strcmp(argv[1],"lease")) leaseVector();
    else if (!strcmp(argv[1],"self-drop")) selfDropVector();
    else if (!strcmp(argv[1],"capture-retirement")) captureRetirement();
    else if (!strcmp(argv[1],"errors")) errors();
    else if (!strcmp(argv[1],"oom")) failures();
    else if (!strcmp(argv[1],"methods")) {
        for (unsigned i=0; i<32; ++i) concurrentMethods();
    }
    else { assert(!strcmp(argv[1],"stress")); stress(); }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Thread %s: genuine resident closure/native worker, lifetimes balanced\n",argv[1]); return 0;
}
