#include "src/runtime/xl6_deadline_service.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
typedef struct Probe {
    XL6_DeadlineTicketV1 ticket;
    XL6_DeadlineServiceV1* service;
    atomic_size_t refs;
    atomic_uint fired,drops,kind,errorKind,sequence;
    atomic_bool entered,go;
    atomic_uint_fast64_t thread;
    xdeadline due;
    bool detach,block,freeOnDrop,selfDestroy;
} Probe;
static atomic_uint sequence;
#ifdef XL6_DEADLINE_SOURCE_FAULT_PROBE
static atomic_uint failWait;
static atomic_uint failMutexCreate,conditionCreates;
xmutex* __real_xrtMutexCreate(void);
xcond* __real_xrtCondCreate(void);
xmutex* __wrap_xrtMutexCreate(void)
{
    unsigned fail=atomic_exchange_explicit(&failMutexCreate,0,memory_order_acq_rel);
    if(fail){xrtClearError();if(fail==1)xrtSetErrorKind(XERR_RANGE);return NULL;}
    return __real_xrtMutexCreate();
}
xcond* __wrap_xrtCondCreate(void)
{
    atomic_fetch_add_explicit(&conditionCreates,1,memory_order_relaxed);
    return __real_xrtCondCreate();
}
xwaitresult __real_xrtCondWaitUntil(xcond*,xmutex*,xdeadline);
xwaitresult __wrap_xrtCondWaitUntil(xcond* condition,xmutex* gate,xdeadline due)
{
    unsigned fail=atomic_exchange_explicit(&failWait,0,memory_order_acq_rel);
    if(fail){xrtClearError();if(fail==1)xrtSetErrorKind(XERR_RANGE);return XWAIT_ERROR;}
    return __real_xrtCondWaitUntil(condition,gate,due);
}
#endif
static void wait_flag(atomic_bool* flag)
{xdeadline until=xrtDeadlineAfter(10000000);while(!atomic_load_explicit(flag,memory_order_acquire)){assert(!xrtDeadlineExpired(until));xrtThreadYield();}}
static void wait_number(atomic_uint* value,unsigned number)
{xdeadline until=xrtDeadlineAfter(10000000);while(atomic_load_explicit(value,memory_order_acquire)!=number){assert(!xrtDeadlineExpired(until));xrtThreadYield();}}
static bool probe_count(const void* data,size_t* count)
{const Probe* probe=data;*count=atomic_load_explicit(&probe->refs,memory_order_relaxed);return *count!=0;}
static bool probe_trace(const void* data,xrtownershipvisitor visit,void* context)
{return visit(xl6DeadlineTicketServiceOwnershipV1(&((const Probe*)data)->ticket),context);}
static const xrtownershipops probeOps={probe_count,probe_trace};
static xrtownershipref probe_ownership(const void* data) {return (xrtownershipref){data,&probeOps};}
static void probe_drop(const void* data)
{
    Probe* probe=(Probe*)data;atomic_fetch_add_explicit(&probe->drops,1,memory_order_relaxed);
    size_t refs=atomic_fetch_sub_explicit(&probe->refs,1,memory_order_acq_rel);assert(refs);
    if(refs==1){assert(probe->freeOnDrop&&xl6DeadlineTicketIsIdleV1(&probe->ticket));xrtFree(probe);}
}
static void notify(const void* data,XL6_DeadlineResultV1 result,const xerror* error)
{
    Probe* probe=(Probe*)data;
    if(result==XL6_DEADLINE_EXPIRED_V1)assert(xrtClock()>=probe->due);
    atomic_store_explicit(&probe->kind,(unsigned)result,memory_order_relaxed);
    atomic_store_explicit(&probe->errorKind,error?(unsigned)xrtErrorKind(error):0,memory_order_relaxed);
    atomic_store_explicit(&probe->thread,xrtThreadCurrentId(),memory_order_relaxed);
    atomic_store_explicit(&probe->sequence,atomic_fetch_add_explicit(&sequence,1,memory_order_relaxed),memory_order_relaxed);
    if(probe->detach)xl6DeadlineDetachV1(&probe->ticket);
    if(probe->selfDestroy)assert(!xl6DeadlineServiceDestroyCompletedV1(probe->service));
    atomic_fetch_add_explicit(&probe->fired,1,memory_order_release);
    atomic_store_explicit(&probe->entered,true,memory_order_release);
    /* Test-only native dispatch barrier; never language CFG or production wait. */
    if(probe->block)wait_flag(&probe->go);
}
static const XL6_DeadlineTargetV1 policy={sizeof(policy),notify,probe_drop,probe_ownership};
static void init(Probe* probe,XL6_DeadlineServiceV1* service,xdeadline due,bool detach)
{memset(probe,0,sizeof(*probe));probe->service=service;probe->due=due;probe->detach=detach;atomic_init(&probe->refs,1);}
static bool bind(Probe* probe)
{
    atomic_fetch_add_explicit(&probe->refs,1,memory_order_relaxed);const void* data=probe;
    bool ok=xl6DeadlineBindTakeV1(probe->service,&probe->ticket,probe->due,&policy,&data);
    assert(ok?!data:data==probe);if(!ok)atomic_fetch_sub_explicit(&probe->refs,1,memory_order_relaxed);return ok;
}
static void finish(XL6_DeadlineServiceV1* service)
{xl6DeadlineServiceCloseV1(service);assert(xl6DeadlineServiceDestroyCompletedV1(service));}
static void balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool edge(xrtownershipref ref,void* count)
{if(ref.Data){assert(ref.Ops);++*(size_t*)count;}return true;}
static void freeze_service(XL6_DeadlineServiceV1* service,xrtownershipscope* freeze)
{
    xdeadline until=xrtDeadlineAfter(10000000);
    for(;;){if(xrtOwnershipFreezeTryBegin(freeze)){size_t count;
            xrtownershipref ref=xl6DeadlineServiceOwnershipV1(service);
            if(ref.Ops->Count(ref.Data,&count))return;
            assert(xrtOwnershipScopeEnd(freeze));}
        assert(!xrtDeadlineExpired(until));xrtThreadYield();}
}
typedef struct Race {
    Probe* probes;
    size_t count;
    atomic_bool* go;
    bool close;
} Race;
static int32_t race_bind_detach(void* data)
{
    Race* race=data;wait_flag(race->go);
    for(size_t i=0;i<race->count;++i){
        if(race->close&&i==race->count/2)xl6DeadlineServiceCloseV1(race->probes[i].service);
        if(!bind(&race->probes[i]))xrtClearError();
        xl6DeadlineDetachV1(&race->probes[i].ticket);
        xl6DeadlineDetachV1(&race->probes[i].ticket);
    }
    return 0;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));unsigned prefix=0;
#ifdef XL6_DEADLINE_SOURCE_FAULT_PROBE
    for(unsigned mode=1;mode<=2;++mode){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        atomic_store(&conditionCreates,0);atomic_store(&failMutexCreate,mode);
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
        XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(16);assert(!service);
        xl6ErrorScopeCaptureFailure(&scope);xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok&&atomic_load(&conditionCreates)==0);
        if(mode==1)assert(outcome.error&&xrtErrorKind(outcome.error)==XERR_RANGE);else assert(!outcome.error);
        xrtErrorFree(outcome.error);balanced(&before);}
#endif
    for(size_t point=0;point<128;++point){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(16);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();if(service)finish(service);balanced(&before);
        if(!hit){prefix=(unsigned)point;break;}assert(point<127);
    }
    // Explicit units, bounded arithmetic, no output mutation on invalid values.
    {xdeadline due=17;assert(!xl6DeadlineFromMillisecondsV1(-1,&due)&&due==17);xrtClearError();
        assert(!xl6DeadlineFromMillisecondsV1(INT64_MAX,&due)&&due==17);xrtClearError();
        uint64_t now=xrtClock();assert(xl6DeadlineFromMillisecondsV1(0,&due)&&due>=now&&due!=XRT_DEADLINE_NEVER);}
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(1);assert(service);
        Probe first,second;init(&first,service,xrtClock()+10000000,false);init(&second,service,0,true);
        assert(bind(&first));assert(!bind(&second)&&xl6DeadlineTicketIsIdleV1(&second.ticket));xrtClearError();
        assert(!xl6DeadlineServiceDestroyV1(service));xl6DeadlineDetachV1(&first.ticket);xl6DeadlineDetachV1(&first.ticket);
        assert(xl6DeadlineTicketIsIdleV1(&first.ticket)&&atomic_load(&first.refs)==1&&atomic_load(&first.drops)==1&&!atomic_load(&first.fired));
        assert(bind(&second));wait_number(&second.drops,1);finish(service);assert(atomic_load(&second.fired)==1);balanced(&before);}
    // A fired ticket still owns Data/service until its owner explicitly retires.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(1);assert(service);
        Probe probe;init(&probe,service,0,false);assert(bind(&probe));wait_number(&probe.fired,1);
        assert(!xl6DeadlineTicketIsIdleV1(&probe.ticket)&&!xl6DeadlineServiceDestroyCompletedV1(service));
        xl6DeadlineServiceCloseV1(service);xl6DeadlineDetachV1(&probe.ticket);finish(service);
        assert(atomic_load(&probe.drops)==1&&atomic_load(&probe.refs)==1);balanced(&before);}
    // Logical detach does not wait for a physical native callback tail or consume
    // capacity. Its Data reference uniquely protects the original ticket.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(1);assert(service);
        Probe first,second;init(&first,service,0,true);first.block=true;first.selfDestroy=true;
        init(&second,service,0,true);assert(bind(&first));wait_flag(&first.entered);
        assert(!xl6DeadlineTicketIsIdleV1(&first.ticket)&&atomic_load(&first.refs)==2);
        assert(bind(&second));xl6DeadlineServiceCloseV1(service);assert(!xl6DeadlineServiceDestroyV1(service));
        atomic_store_explicit(&first.go,true,memory_order_release);wait_number(&second.drops,1);finish(service);
        assert(atomic_load(&first.drops)==1&&atomic_load(&second.fired)==1);balanced(&before);}
    // Drop can free the unique allocation CONTAINING the ticket: no post-Drop access.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(1);assert(service);
        Probe* probe=xrtCalloc(1,sizeof(*probe));assert(probe);init(probe,service,0,true);probe->freeOnDrop=true;
        const void* data=probe;assert(xl6DeadlineBindTakeV1(service,&probe->ticket,0,&policy,&data)&&!data);
        xl6DeadlineServiceCloseV1(service);
        xdeadline until=xrtDeadlineAfter(10000000);while(!xl6DeadlineServiceDestroyV1(service)){assert(!xrtDeadlineExpired(until));xrtThreadYield();}
        balanced(&before);}
    // No allocation between binding, expiration/reentrant detach, and delivery.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(1);assert(service);
        Probe probe;init(&probe,service,xrtClock()+10000,true);assert(xrtMemDebugFailAfter(0));assert(bind(&probe));
        wait_number(&probe.drops,1);finish(service);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();balanced(&before);}
    // Min-heap ordering, reverse insertion, complete target reference accounting.
    {enum{N=32};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(N);assert(service);
        Probe probes[N];atomic_store(&sequence,0);uint64_t start=xrtClock()+100000;
        for(size_t i=0;i<N;++i)init(&probes[i],service,start+i*1000,true);
        for(size_t i=N;i;--i)assert(bind(&probes[i-1]));
        xl6DeadlineServiceCloseV1(service);for(size_t i=0;i<N;++i)wait_number(&probes[i].drops,1);finish(service);
        uint64_t thread=atomic_load(&probes[0].thread);assert(thread&&thread!=xrtThreadCurrentId());
        for(size_t i=0;i<N;++i)assert(atomic_load(&probes[i].fired)==1&&atomic_load(&probes[i].sequence)==i&&
            atomic_load(&probes[i].thread)==thread&&atomic_load(&probes[i].refs)==1);balanced(&before);}
    // 1024 concurrent future deadlines; real catalog, exact owning graph, and
    // arbitrary removal/reuse exercise both upward and downward heap repair.
    {enum{N=1024};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(N);assert(service);
        Probe probes[N];for(size_t i=0;i<N;++i){init(&probes[i],service,xrtClock()+10000000+(i*73)%N,false);assert(bind(&probes[i]));}
        const XL6_OwnershipDomainV1* domain=xl6OwnershipDomainDeadlineServiceV1Get();
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(&domain,1);assert(catalog);
        xrtownershipscope freeze={0};freeze_service(service,&freeze);xrtownershipref ref=xl6DeadlineServiceOwnershipV1(service);
        size_t count=0,edges=0;assert(ref.Ops->Count(ref.Data,&count)&&count==N+1&&ref.Ops->Trace(ref.Data,edge,&edges)&&edges==N);
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finishPolicy=NULL;
        assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&finishPolicy)&&adapter&&preparation&&!finishPolicy);
        int token=0;assert(!adapter->Claim(ref.Data,&token));assert(adapter->Hold(ref.Data));
        xrtownershipresult graph={0};assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==N+1);
        assert(xrtOwnershipScopeEnd(&freeze));
        for(size_t i=0;i<N;++i){size_t at=(i*73)%N;xl6DeadlineDetachV1(&probes[at].ticket);assert(atomic_load(&probes[at].drops)==1);}
        for(size_t i=0;i<32;++i){probes[i].due=0;probes[i].detach=true;assert(bind(&probes[i]));wait_number(&probes[i].drops,2);}
        finish(service);adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);}
    // Claim/Restore reopens admission. Prepare joins only an empty source;
    // Clear is separate from the owner's terminal destruction and final pin.
    {xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(1);assert(service);
        xrtownershipscope freeze={0};freeze_service(service,&freeze);xrtownershipref ref=xl6DeadlineServiceOwnershipV1(service);
        const xrtownershippreparationv1* preparation=NULL;
        const xrtownershipadapterv1* adapter=xl6DeadlineServiceOwnershipAdapterV1(ref,&preparation);assert(adapter&&preparation);
        int token=0;assert(adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
        assert(xrtOwnershipScopeEnd(&freeze));Probe probe;init(&probe,service,xrtClock()+10000000,false);
        assert(bind(&probe));xl6DeadlineDetachV1(&probe.ticket);
        freeze_service(service,&freeze);assert(adapter->Claim(ref.Data,&token));assert(xrtOwnershipScopeEnd(&freeze));
        assert(!xl6DeadlineServiceDestroyV1(service)&&!xl6DeadlineServiceDestroyCompletedV1(service));
        assert(!bind(&probe)&&xl6DeadlineTicketIsIdleV1(&probe.ticket));xrtClearError();
        xdeadline until=xrtDeadlineAfter(10000000);
        for(;;){xrtownershipprepareresult result=preparation->Prepare(ref.Data,&token);
            if(result==XRT_OWNERSHIP_PREPARE_READY)break;
            assert(result==XRT_OWNERSHIP_PREPARE_BUSY&&!xrtDeadlineExpired(until));xrtThreadYield();}
        freeze_service(service,&freeze);assert(preparation->Ready(ref.Data));adapter->Clear(ref.Data,&token);
        assert(xrtOwnershipScopeEnd(&freeze));assert(xl6DeadlineServiceDestroyCompletedV1(service));
        adapter->Drop(ref.Data);balanced(&before);}
    // Actual simultaneous registration, expiration/reentrant detach, duplicate
    // detach and Close. Threads own disjoint ticket storage; service is shared.
    for(unsigned round=0;round<16;++round){enum{WORKERS=4,N=32};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(WORKERS*N);assert(service);
        Probe probes[WORKERS*N];Race races[WORKERS];xthread* threads[WORKERS];atomic_bool go=false;
        for(size_t i=0;i<WORKERS*N;++i)init(&probes[i],service,i%2?0:xrtClock()+10000000,true);
        for(size_t i=0;i<WORKERS;++i){races[i]=(Race){probes+i*N,N,&go,i==0};threads[i]=xrtThreadCreate(race_bind_detach,&races[i],0);assert(threads[i]);}
        atomic_store_explicit(&go,true,memory_order_release);
        for(size_t i=0;i<WORKERS;++i){assert(xrtThreadWait(threads[i])==XWAIT_OK);xrtThreadDestroy(threads[i]);}
        finish(service);
        for(size_t i=0;i<WORKERS*N;++i)assert(xl6DeadlineTicketIsIdleV1(&probes[i].ticket)&&atomic_load(&probes[i].refs)==1&&
            atomic_load(&probes[i].drops)<=1&&atomic_load(&probes[i].fired)<=atomic_load(&probes[i].drops));
        balanced(&before);}
#ifdef XL6_DEADLINE_SOURCE_FAULT_PROBE
    // Native wait failure is FAILED (including false/NULL), never fake expiration.
    for(unsigned mode=1;mode<=2;++mode){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_DeadlineServiceV1* service=xl6DeadlineServiceCreateV1(2);assert(service);Probe probe;
        init(&probe,service,xrtClock()+10000000,true);assert(bind(&probe));
        xrtownershipscope freeze={0};freeze_service(service,&freeze);atomic_store(&failWait,mode);assert(xrtOwnershipScopeEnd(&freeze));
        xl6DeadlineServiceCloseV1(service);wait_number(&probe.drops,1);
        assert(atomic_load(&probe.kind)==XL6_DEADLINE_FAILED_V1&&atomic_load(&probe.errorKind)==(mode==1?XERR_RANGE:0));
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));bool ok=xl6DeadlineServiceCheckV1(service);
        if(!ok)xl6ErrorScopeCaptureFailure(&scope);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);assert(!ok&&!outcome.ok);
        if(mode==1)assert(outcome.error&&xrtErrorKind(outcome.error)==XERR_RANGE);else assert(!outcome.error);
        xrtErrorFree(outcome.error);xrtClearError();finish(service);balanced(&before);}
    puts("source-only first-create and native-wait error/false-NULL failure passed");
#endif
    printf("generic deadline source: one worker/1024 pending; heap order/remove/reuse; full/close/units/overflow; real catalog/lease graph/pins; claim/restore/prepare/clear; 2048 registration/detach/close races; reentrant detach, capacity independent physical tail, unique contained ticket Drop; no late allocation; create prefix %u; balanced\n",prefix);
    return 0;
}
