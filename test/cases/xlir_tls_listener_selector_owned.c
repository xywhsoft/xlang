/* Ordinary literal-loopback listener lifecycle, without client traffic.
 * A real extra native reference models delayed aggregate Data retirement.
 * Capture cleanup must run on the checked source tail, not the Data thread. */
#include "release/include/xl_clock.h"
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_execution_tls_internal.h"
#include "src/runtime/xl6_tls_role_context_internal.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>

typedef struct Capture {xatomic32 refs,drops;uint64 thread;unsigned failure;} Capture;
static void mutation(xrtownershipscope* p){assert(xrtOwnershipMutationBegin(p));}
static void done(xrtownershipscope* p){assert(xrtOwnershipScopeEnd(p));}
static void* retain(void* raw)
{Capture* p=raw;xrtownershipscope m={0};mutation(&m);assert(xrtAtomic32FetchAdd(&p->refs,1,XMEMORY_ACQ_REL)>0);done(&m);return p;}
static void drop(void* raw)
{
    Capture* p=raw;xrtownershipscope m={0};mutation(&m);uint32 previous=xrtAtomic32FetchSub(&p->refs,1,XMEMORY_ACQ_REL);assert(previous);done(&m);
    if(previous==1){
        p->thread=xrtThreadCurrentId();xrtAtomic32FetchAdd(&p->drops,1,XMEMORY_RELEASE);
        if(p->failure){if(p->failure==1)xrtSetErrorKind(XERR_RANGE);xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
    }
}
static bool count(const void* raw,size_t* output)
{if(!raw||!output)return false;uint32 n=xrtAtomic32Load(&((const Capture*)raw)->refs,XMEMORY_ACQUIRE);*output=n;return n>0;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{size_t n;(void)context;return visit&&count(raw,&n);}
static const xrtownershipops ownershipOps={count,trace};
static xrtownershipref ownership(const void* p){return (xrtownershipref){p,&ownershipOps};}
static bool unusedSelect(ptr raw,const xtlsserverrequest* request,xtlsserverchoice* choice)
{(void)raw;(void)request;(void)choice;assert(0);return false;}
static const XL6_TlsSelectorOpsV1 selectorOps={retain,drop,ownership,unusedSelect};
static bool unusedLookup(ptr raw,const xtlsserverresumerequest* request,xtlsresume** output)
{(void)raw;(void)request;(void)output;assert(0);return false;}
static const XL6_TlsResumeLookupOpsV1 lookupOps={retain,drop,ownership,unusedLookup};
typedef struct Probe {Capture* capture;size_t edges;} Probe;
static bool inspect(xrtownershipref ref,void* raw)
{Probe* p=raw;if(ref.Ops==&ownershipOps){assert(ref.Data==p->capture);++p->edges;}return true;}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);
}
static void roundTrip(unsigned failure)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Capture capture={0};capture.failure=failure;xrtAtomic32Init(&capture.refs,1);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    xnetengineconfig options;xrtNetEngineConfigInit(&options);options.Backend=XNET_PORT_SELECT;options.Workers=1;
    XL6_ExecutionResourceV1* engine=xl6ExecutionNetEngineCreateV1(source,&options);assert(engine&&xl6ExecutionNetEngineStartV1(engine));
    XL6_TlsServerContextV1* context=xl6TlsServerContextFromCallbacksV1(NULL,NULL,NULL,false,
        (XL6_TlsSelectorV1){&capture,&selectorOps},(XL6_TlsResumeLookupV1){&capture,&lookupOps},10000);assert(context);
    xtlslistenerconfig config;xrtTlsListenerConfigInit(&config);assert(xrtNetAddrLoopback(&config.Listen.Address,XNET_FAMILY_IPV4,0));
    XL6_ExecutionResourceV1* listener=xl6ExecutionTlsListenV1(source,engine,context,&config);assert(listener);
    xl6TlsServerContextDestroyV1(context);drop(&capture);assert(xrtAtomic32Load(&capture.refs,XMEMORY_ACQUIRE)==2);
    XL6_TlsScopeV1 scope={0};assert(xl6ExecutionTlsListenerEnterV1(listener,false,&scope));
    xtlslistener* held=xrtTlsListenerRef(scope.listener);assert(held);xl6ExecutionTlsLeaveV1(&scope);
    assert(xl6ExecutionResourceCloseV1(listener));xl6ExecutionResourceReleaseV1(listener);
    uint64 deadline=xl6MonotonicMillisecondsV1()+UINT64_C(10000000);XL6_ExecutionResourceSnapshotV1 snapshot={.size=sizeof(snapshot)};
    for(;;){
        assert(xl6ExecutionResourceSnapshotV1Get(listener,&snapshot));
        if(!snapshot.nativePresent)break;
        assert(xl6MonotonicMillisecondsV1()<deadline);xrtSleep(1);
    }
    assert(!snapshot.owners&&!snapshot.stalled&&!xrtAtomic32Load(&capture.drops,XMEMORY_ACQUIRE));
    /* Once the creator is detached, the actual native Data owner still keeps
     * this exact source record and immutable selector graph edge alive. */
    bool observed=false;
    while(!observed){
        xrtownershipscope frozen={0};
        if(xrtOwnershipFreezeTryBegin(&frozen)){
            xrtownershipref ref=xl6ExecutionResourceOwnershipV1(listener);size_t references=0;
            if(ref.Ops->Count(ref.Data,&references)){
                Probe probe={&capture,0};assert(references>=2&&ref.Ops->Trace(ref.Data,inspect,&probe)&&probe.edges==2);observed=true;
            }
            done(&frozen);
        }
        assert(xl6MonotonicMillisecondsV1()<deadline);if(!observed)xrtSleep(1);
    }
    XL6_ExecutionSourceSnapshotV1 status={.size=sizeof(status)};
    assert(xl6ExecutionSourceSnapshotV1Get(source,&status)&&status.resources==2&&!status.failed);
    xrtTlsListenerDestroy(held); /* Data drop only signals; no capture cleanup. */
    assert(!xrtGetError());
    while(!xrtAtomic32Load(&capture.drops,XMEMORY_ACQUIRE)){assert(xl6MonotonicMillisecondsV1()<deadline);xrtSleep(1);}
    assert(capture.thread!=xrtThreadCurrentId()&&!xrtAtomic32Load(&capture.refs,XMEMORY_ACQUIRE));
    assert(xl6ExecutionResourceCloseV1(engine));xl6ExecutionResourceReleaseV1(engine);assert(xl6ExecutionSourceCloseV1(source));
    XL6_ExecutionRetireResultV1 result={0};
    do{result=xl6ExecutionSourceDestroyV1(&source);if(!result.ready){assert(source&&xl6MonotonicMillisecondsV1()<deadline);xrtClearError();xrtSleep(1);}}while(!result.ready);
    assert(!source&&result.ok==(failure==0));
    if(failure==1)assert(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_RANGE);
    else assert(!xrtGetError());
    xrtClearError();balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    for(unsigned i=0;i<3;++i)roundTrip(i);
    puts("TLS listener selector: aggregate Data lifetime, two selector/resume graph edges, deferred checked cleanup with/without diagnostic, balanced: ok");return 0;
}
