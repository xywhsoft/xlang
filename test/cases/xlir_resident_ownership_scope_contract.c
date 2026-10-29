#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_continuation_future.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const XL6_ClosureRuntimeV1* closures;
static const XL6_ClassRuntimeV1* classes;
static void require(bool ok, const char* message)
{ if (!ok) { fprintf(stderr, "[FAIL] %s\n", message); exit(3); } }
static LONG read_flag(volatile LONG* value) { return InterlockedCompareExchange(value,0,0); }
static void await_flag(volatile LONG* value)
{
    DWORD started=GetTickCount();
    while (!read_flag(value)) { require((DWORD)(GetTickCount()-started)<10000,"native gate deadline"); SwitchToThread(); }
}
typedef struct Gate { volatile LONG entered, proceed, armed; DWORD thread; } Gate;
static Gate* allocation_gate;
static void hold(Gate* gate)
{ InterlockedExchange(&gate->entered,1); await_flag(&gate->proceed); }
static void* allocate(void* unused, size_t size)
{
    void* result=malloc(size); Gate* gate=allocation_gate; (void)unused;
    if (gate && GetCurrentThreadId()==gate->thread && InterlockedExchange(&gate->armed,0)) hold(gate);
    return result;
}
static void* reallocate(void* unused, void* value, size_t size)
{ (void)unused; return realloc(value,size); }
static void deallocate(void* unused, void* value) { (void)unused; free(value); }
typedef struct Owner {
    XL6_CodeOwnerGraph descriptor;
    volatile LONG pins, acquired, released, closed;
    Gate* releaseGate;
} Owner;
static int acquire(void* data)
{
    Owner* owner=data;
    if (read_flag(&owner->closed)) return 0;
    InterlockedIncrement(&owner->pins); InterlockedIncrement(&owner->acquired); return 1;
}
static void release(void* data)
{
    Owner* owner=data;
    require(InterlockedDecrement(&owner->pins)>=0,"real owner count");
    InterlockedIncrement(&owner->released);
    if (owner->releaseGate) hold(owner->releaseGate);
}
static bool owner_count(const void* data, size_t* count)
{
    Owner* owner=(Owner*)data;
    *count=(size_t)read_flag(&owner->pins)+(read_flag(&owner->closed)?0u:1u); return true;
}
static bool no_edges(const void* data, XL6_ModuleOwnershipVisit visit, void* context)
{ (void)data; (void)visit; (void)context; return true; }
static const xrtownershipops owner_ops={owner_count,no_edges};
typedef struct Fixture {
    Owner owner;
    XL6_CodeLease *root, *other;
    XL6_Closure* closure;
    XL6_CodeDrop* record;
    xvalue* value;
    void* payload;
    Gate gate;
    volatile LONG ready, go, done, finish;
    unsigned kind, drops;
} Fixture;
static void invoke(void) {}
static bool box_entry(void* data, const xrtcallframe* frame, xrtcallresult* result)
{ (void)data; (void)frame; (void)result; return true; }
static void drop(void* value, void* data)
{ Fixture* fixture=data; (void)value; ++fixture->drops; }
static void callback(Fixture* fixture)
{
    hold(&fixture->gate);
    require(xl6CodeLeaseRetain(fixture->root)==fixture->root,"callback nested lease retain");
    xl6CodeLeaseRelease(fixture->root); ++fixture->drops;
}
static void environment_drop(void* data) { callback(data); }
static void blocked_drop(void* value, void* data) { (void)value; callback(data); }
static Fixture* finalizing;
static void finalize(xvalue* value, void* data) { (void)value; (void)data; callback(finalizing); }
static void quiet_finalize(xvalue* value, void* data) { (void)value; (void)data; }
static bool empty_scope(const xrtownershipscope* scope)
{ return !scope->Self && !scope->Parent && !scope->Thread && !scope->Children && !scope->Mode; }
static void begin_fixture(Fixture* fixture, xmemdebugsnapshot* before)
{
    xrtMemDebugSnapshot(before); memset(fixture,0,sizeof(*fixture));
    fixture->owner.descriptor=(XL6_CodeOwnerGraph){
        {sizeof(XL6_CodeOwnerGraph),&fixture->owner,acquire,release},0x584C36434F574E31ULL,&owner_ops};
    require(xl6CodeLeaseAcquire(&fixture->owner.descriptor.base,&fixture->root)==XL6_CODE_LEASE_ACQUIRED,
        "fixture root admission");
    XL6_CodeScope scope={0}; xl6CodeScopeEnter(&scope,fixture->root);
    fixture->closure=closures->Create(&fixture->owner.descriptor.base,invoke,NULL,NULL,NULL,NULL,NULL,0,NULL);
    xl6CodeScopeLeave(&scope); require(fixture->closure!=NULL,"fixture closure");
}
static void end_fixture(Fixture* fixture, const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    fixture->owner.releaseGate=NULL;
    xl6CodeDropAbort(fixture->record); xl6CodeLeaseRelease(fixture->other);
    closures->Release(fixture->closure); xrtValueRelease(fixture->value); xrtFree(fixture->payload);
    xl6CodeLeaseRelease(fixture->root);
    require(!read_flag(&fixture->owner.pins) && read_flag(&fixture->owner.acquired)==read_flag(&fixture->owner.released),
        "all real code acquisitions released exactly once");
    require(xrtGetError()==NULL,"no residual resident transition diagnostic");
    xrtMemDebugSnapshot(&after);
    require(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount,
        "per-transaction exact allocator balance");
}
static HANDLE start(LPTHREAD_START_ROUTINE proc, Fixture* fixture)
{ HANDLE thread=CreateThread(NULL,0,proc,fixture,0,NULL); require(thread!=NULL,"native worker"); return thread; }
static void join(HANDLE thread)
{
    DWORD result;
    require(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"native worker returned");
    require(GetExitCodeThread(thread,&result) && result==0,"native worker result"); CloseHandle(thread);
}
static DWORD WINAPI transition(void* data)
{
    Fixture* f=data; XL6_CodeScope code={0}; XL6_ClosureScope closure={0};
    if (f->kind==4 || f->kind==5) xl6CodeScopeEnter(&code,f->root);
    if (f->kind==9) closures->Enter(&closure,f->closure);
    InterlockedExchange(&f->ready,1); await_flag(&f->go);
    xrtownershipscope refused={0};
    require(!xrtOwnershipFreezeTryBegin(&refused) && empty_scope(&refused),"native worker observes existing freeze");
    InterlockedExchange(&f->ready,2);
    switch (f->kind) {
    case 0: require(xl6CodeLeaseAcquire(&f->owner.descriptor.base,&f->other)==XL6_CODE_LEASE_ACQUIRED,"new lease"); break;
    case 1: f->other=xl6CodeLeaseRetain(f->root); break;
    case 2: xl6CodeLeaseRelease(f->other); f->other=NULL; break;
    case 3: xl6CodeScopeEnter(&code,f->root); break;
    case 4: xl6CodeScopeLeave(&code); break;
    case 5: require(xl6CodeLeaseAcquireScoped(&f->owner.descriptor.base,&f->other)==XL6_CODE_LEASE_ACQUIRED,"inherit after Close"); break;
    case 6: f->record=xl6CodeDropCreate(f->root,drop,f); require(f->record!=NULL,"drop transfer"); break;
    case 7: xl6CodeDropAbort(f->record); f->record=NULL; break;
    case 8: closures->Enter(&closure,f->closure); break;
    case 9: closures->Leave(&closure); break;
    default: require(false,"transition kind");
    }
    InterlockedExchange(&f->done,1); await_flag(&f->finish);
    if (code.lease) xl6CodeScopeLeave(&code);
    if (closure.closure) closures->Leave(&closure);
    return 0;
}
static void frozen_transition(unsigned kind)
{
    Fixture f; xmemdebugsnapshot before; xrtownershipscope freeze={0};
    XL6_CodeScope mainScope={0}; xrtownershipref anchor; xrtownershipresult graph={0},during={0};
    size_t count, observed; LONG pins; HANDLE thread;
    begin_fixture(&f,&before); f.kind=kind;
    if (kind==2) f.other=xl6CodeLeaseRetain(f.root);
    if (kind==7) { f.record=xl6CodeDropCreate(f.root,drop,&f); require(f.record!=NULL,"existing drop"); }
    xl6CodeScopeEnter(&mainScope,f.root);
    thread=start(transition,&f); await_flag(&f.ready);
    if (kind==5) InterlockedExchange(&f.owner.closed,1);
    anchor=xl6CodeLeaseOwnership(f.root);
    require(xrtOwnershipFreezeTryBegin(&freeze) && anchor.Ops->Count(anchor.Data,&count) &&
        xrtOwnershipInspect(&anchor,1,NULL,0,&graph),"freeze actual lease graph");
    pins=read_flag(&f.owner.pins); InterlockedExchange(&f.go,1);
    while(read_flag(&f.ready)!=2) SwitchToThread();
    for(unsigned i=0;i<2000;++i) SwitchToThread();
    require(!read_flag(&f.done),"entire resident ownership transition waits outside freeze");
    require(anchor.Ops->Count(anchor.Data,&observed) && observed==count && read_flag(&f.owner.pins)==pins &&
        xrtOwnershipInspect(&anchor,1,NULL,0,&during) && !memcmp(&graph,&during,sizeof(graph)),"frozen exact counts and edges");
    /* If AcquireScoped waits at Retain while holding the stack-list lock, this
     * frozen owner's ordinary nested lookup deadlocks. Admission must be first. */
    XL6_CodeLease* probe=NULL;
    require(xl6CodeLeaseAcquireScoped(&f.owner.descriptor.base,&probe)==XL6_CODE_LEASE_ACQUIRED && probe==f.root,
        "no stack-list lock is held by a frozen writer");
    xl6CodeLeaseRelease(probe);
    require(xrtOwnershipScopeEnd(&freeze),"end frozen transition");
    await_flag(&f.done);
    require(xrtOwnershipFreezeTryBegin(&freeze) && anchor.Ops->Count(anchor.Data,&observed),"observe completed transition");
    int delta=kind==0?0:(kind==2 || kind==4 || kind==7 || kind==9)?-1:1;
    require(observed==(size_t)((long long)count+delta) && read_flag(&f.owner.pins)==pins+(kind==0?1:0),"exact completed ownership delta");
    require(xrtOwnershipScopeEnd(&freeze),"end observation");
    InterlockedExchange(&f.finish,1); join(thread); xl6CodeScopeLeave(&mainScope);
    require(f.drops==0,"aborted records never invoke drop"); end_fixture(&f,&before);
}
static DWORD WINAPI in_flight(void* data)
{
    Fixture* f=data; XL6_CodeScope scope={0};
    if (f->kind>=5) {
        xl6CodeScopeEnter(&scope,f->root);
        f->gate.thread=GetCurrentThreadId(); InterlockedExchange(&f->gate.armed,1);
    }
    switch(f->kind) {
    case 0: xl6CodeLeaseRelease(f->other); f->other=NULL; break;
    case 1: closures->Release(f->closure); f->closure=NULL; break;
    case 2: xl6CodeDropInvoke(NULL,f->record); f->record=NULL; break;
    case 3: closures->PayloadDrop(f->payload,NULL); f->payload=NULL; break;
    case 4: xrtValueRelease(f->value); f->value=NULL; break;
    case 5: {
        XL6_Closure* child=closures->Create(&f->owner.descriptor.base,invoke,NULL,NULL,NULL,NULL,NULL,0,NULL);
        require(child!=NULL,"factory closure"); closures->Release(child); break;
    }
    case 6: f->value=closures->Box(&f->owner.descriptor.base,f->closure,box_entry,168); require(f->value!=NULL,"factory box"); break;
    case 7: f->value=classes->Create(&f->owner.descriptor.base,NULL); require(f->value!=NULL,"factory class"); break;
    case 8: require(xl6CodeFinalizerBind(f->value,f->root,quiet_finalize,NULL,NULL,NULL),"factory finalizer binding"); break;
    case 9: f->record=xl6CodeDropCreate(f->root,drop,f); require(f->record!=NULL,"factory drop"); break;
    default: require(false,"in-flight kind");
    }
    if (scope.lease) xl6CodeScopeLeave(&scope);
    return 0;
}
static void callback_or_factory(unsigned kind)
{
    Fixture f; xmemdebugsnapshot before; HANDLE thread;
    begin_fixture(&f,&before); f.kind=kind;
    if (kind==0) {
        require(xl6CodeLeaseAcquire(&f.owner.descriptor.base,&f.other)==XL6_CODE_LEASE_ACQUIRED,"independent last lease");
        f.owner.releaseGate=&f.gate;
    }
    if (kind==1 || kind==3) {
        closures->Release(f.closure);
        f.closure=closures->Create(&f.owner.descriptor.base,invoke,NULL,&f,environment_drop,NULL,NULL,0,no_edges);
        require(f.closure!=NULL,"owned environment callback");
        if (kind==3) {
            f.payload=xrtMalloc(sizeof(XL6_Closure*)); require(f.payload!=NULL,"owning payload cell");
            *(XL6_Closure**)f.payload=f.closure; f.closure=NULL;
        }
    }
    if (kind==2) { f.record=xl6CodeDropCreate(f.root,blocked_drop,&f); require(f.record!=NULL,"callback drop"); }
    if (kind==4) {
        finalizing=&f; f.value=classes->Create(&f.owner.descriptor.base,finalize);
        require(f.value!=NULL && classes->CommitTake(f.value)==f.value,"committed finalizing class");
    }
    if (kind==8) { f.value=xrtValueObjectLifo(); require(f.value!=NULL,"bind receiver"); }
    if (kind>=5) allocation_gate=&f.gate;
    thread=start(in_flight,&f); await_flag(&f.gate.entered);
    for(unsigned i=0;i<100;++i) {
        xrtownershipscope refused={0};
        if(kind==0 || kind==4) {
            /* The physical lease is gone; its host release is an outer tail,
             * allowed to freeze or join workers. A certified class finalizer
             * also runs outside the terminal RC transition, but its receiver
             * must be unavailable to graph admission throughout the callback.
             * Legacy drop callbacks and factory transfers remain protected. */
            require(xrtOwnershipFreezeTryBegin(&refused) && xrtGetError()==NULL,
                "host retirement tail does not retain global mutation admission");
            if(kind==4) {
                size_t count=181; xrtownershipref receiver=xrtValueOwnership(f.value);
                require(!receiver.Ops->Count(receiver.Data,&count) && count==181 &&
                    !xl6ClassOwnershipAdapterV1(receiver) && read_flag(&f.owner.pins)>0,
                    "terminal class receiver refused while real code owners remain alive");
            }
            require(xrtOwnershipScopeEnd(&refused),"end host-tail observation");
        } else require(!xrtOwnershipFreezeTryBegin(&refused) && empty_scope(&refused) && xrtGetError()==NULL,
            "whole resident callback/factory boundary refuses freeze");
        xl6CodeLeaseRelease(xl6CodeLeaseRetain(f.root));
    }
    InterlockedExchange(&f.gate.proceed,1); join(thread); allocation_gate=NULL;
    require(f.drops==(kind>=1 && kind<=4?1u:0u),"exact callback count");
    end_fixture(&f,&before);
}
typedef struct Invocation { Fixture* fixture; xfuture* future; volatile LONG ready; } Invocation;
static DWORD WINAPI pending_invocation(void* data)
{
    Invocation* work=data; XL6_ClosureScope scope={0};
    closures->Enter(&scope,work->fixture->closure); InterlockedExchange(&work->ready,1);
    require(xrtFutureWait(work->future)==XWAIT_OK,"actual pending invocation resumes");
    closures->Leave(&scope); return 0;
}
static void waiting_invocation(void)
{
    Fixture f; xmemdebugsnapshot before; Invocation work={0}; xfuture* future=NULL;
    begin_fixture(&f,&before); xpromise* promise=xrtPromiseCreate(&future,NULL); require(promise!=NULL,"pending future");
    work.fixture=&f; work.future=future;
    HANDLE thread=CreateThread(NULL,0,pending_invocation,&work,0,NULL); require(thread!=NULL,"pending native invocation");
    await_flag(&work.ready);
    bool observed=false; DWORD started=GetTickCount();
    while(!observed && (DWORD)(GetTickCount()-started)<5000) {
        xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)) {
            size_t count=0,refs=0; xrtownershipref view=xrtFutureOwnership(future),lease=xl6CodeLeaseOwnership(f.root);
            observed=view.Ops->Count(view.Data,&count) && count==3 &&
                lease.Ops->Count(lease.Data,&refs) && refs==3 && xrtFutureState(future)==XFUTURE_PENDING;
            require(xrtOwnershipScopeEnd(&freeze),"end pending inspection");
        }
        SwitchToThread();
    }
    /* Always release the real waiter before reporting a failed observation. */
    require(xrtPromiseResolve(promise,NULL),"resolve pending invocation"); join(thread);
    require(observed,"ordinary invocation/park does not monopolize mutation domain; actual roots remain counted");
    xrtPromiseDestroy(promise); xrtFutureDestroy(future); end_fixture(&f,&before);
}
typedef struct QueueWork {
    Fixture* fixture;
    XL6_ContinuationQueue* queue;
    XL6_ContinuationTicket ticket;
    xfuturewatch watch;
    xfuture* future;
    xpromise* promise;
    XL6_Closure* child;
    Gate releaseGate;
    volatile LONG ready, go, done;
    unsigned kind, runs, releases;
} QueueWork;
static void queue_run(void* data) { ++((QueueWork*)data)->runs; }
static void queue_release(void* data) { ++((QueueWork*)data)->releases; }
static void reserve_ticket(QueueWork* work)
{
    require(xl6ContinuationReserveOwned(work->queue,&work->ticket,queue_run,queue_release,work,
        &work->fixture->owner.descriptor.base)==XL6_CONTINUATION_ADMITTED,"owned queue reservation");
}
static DWORD WINAPI queue_transition(void* data)
{
    QueueWork* w=data; InterlockedExchange(&w->ready,1); await_flag(&w->go);
    xrtownershipscope refused={0};
    require(!xrtOwnershipFreezeTryBegin(&refused) && empty_scope(&refused),"queue worker observes freeze");
    InterlockedExchange(&w->ready,2);
    switch(w->kind) {
    case 0: require(xl6ContinuationReserve(w->queue,&w->ticket,queue_run,queue_release,w)==XL6_CONTINUATION_ADMITTED,"unowned reservation"); break;
    case 1: reserve_ticket(w); break;
    case 2: require(xl6ContinuationTraceBind(&w->ticket,no_edges),"ticket trace publication"); break;
    case 3: xl6ContinuationCommit(&w->ticket); break;
    case 4: xl6ContinuationReady(&w->ticket); break;
    case 5: xl6ContinuationTransportReleased(&w->ticket); break;
    case 6: require(xl6ContinuationAbort(&w->ticket),"ticket abort"); break;
    case 7: require(xl6ContinuationWatchRegister(&w->ticket,&w->watch,w->future),"transport owner publication"); break;
    case 8: require(xl6ContinuationRunOne(w->queue),"ticket dispatch"); break;
    case 9: xl6ContinuationQueueClose(w->queue); break;
    default: require(false,"queue transition kind");
    }
    InterlockedExchange(&w->done,1); return 0;
}
static void frozen_queue_transition(unsigned kind)
{
    Fixture f; xmemdebugsnapshot before; QueueWork w={0}; XL6_ContinuationTicket saved;
    xrtownershipscope freeze={0}; size_t active; LONG pins;
    begin_fixture(&f,&before); w.fixture=&f; w.kind=kind;
    w.queue=xl6ContinuationQueueCreate(2); require(w.queue!=NULL,"queue fixture");
    if(kind>=2 && kind<=8) {
        reserve_ticket(&w);
        if(kind!=2) require(xl6ContinuationTraceBind(&w.ticket,no_edges),"private ticket trace");
        if(kind==4 || kind==5 || kind==8) xl6ContinuationCommit(&w.ticket);
        if(kind==5 || kind==8) xl6ContinuationReady(&w.ticket);
        if(kind==8) xl6ContinuationTransportReleased(&w.ticket);
    }
    if(kind==7) { w.promise=xrtPromiseCreate(&w.future,NULL); require(w.promise!=NULL,"transport future"); }
    HANDLE thread=CreateThread(NULL,0,queue_transition,&w,0,NULL); require(thread!=NULL,"queue native transition");
    await_flag(&w.ready); saved=w.ticket; active=xl6ContinuationQueueActivationCount(w.queue); pins=read_flag(&f.owner.pins);
    require(xrtOwnershipFreezeTryBegin(&freeze),"queue transition freeze"); InterlockedExchange(&w.go,1);
    while(read_flag(&w.ready)!=2) SwitchToThread();
    for(unsigned i=0;i<2000;++i) SwitchToThread();
    require(!read_flag(&w.done),"entire queue/transport transition waits outside freeze");
    require(!memcmp(&saved,&w.ticket,sizeof(saved)) && active==xl6ContinuationQueueActivationCount(w.queue) &&
        pins==read_flag(&f.owner.pins),"frozen physical ticket, credit and code edges");
    require(xrtOwnershipScopeEnd(&freeze),"end queue freeze"); join(thread);
    switch(kind) {
    case 0: case 1: case 2: require(xl6ContinuationAbort(&w.ticket),"cleanup unpublished reservation"); break;
    case 3: xl6ContinuationReady(&w.ticket); xl6ContinuationTransportReleased(&w.ticket); break;
    case 4: xl6ContinuationTransportReleased(&w.ticket); break;
    case 7: xl6ContinuationCommit(&w.ticket); require(xrtPromiseResolve(w.promise,NULL),"complete actual transport"); break;
    default: break;
    }
    if(kind==3 || kind==4 || kind==5 || kind==7) require(xl6ContinuationRunOne(w.queue),"dispatch ready cleanup");
    bool dispatched=kind==3 || kind==4 || kind==5 || kind==7 || kind==8;
    require(w.runs==(dispatched?1u:0u) && w.releases==w.runs && xl6ContinuationQueueIsIdle(w.queue),
        "queue exact execution/physical-release counts");
    xl6ContinuationQueueClose(w.queue); require(xl6ContinuationQueueDestroy(w.queue),"closed empty queue destroyed");
    xrtPromiseDestroy(w.promise); xrtFutureDestroy(w.future); end_fixture(&f,&before);
}
static void queue_pending_run(void* data)
{
    QueueWork* w=data; ++w->runs; InterlockedExchange(&w->ready,1);
    require(xrtFutureWait(w->future)==XWAIT_OK,"continuation actual pending wait resumed");
}
static void queue_pending_release(void* data)
{
    QueueWork* w=data; hold(&w->releaseGate);
    /* After Close, release is still executing accepted code. A new closure
     * inherits that same physical lease without reopening owner admission. */
    w->child=closures->Create(&w->fixture->owner.descriptor.base,invoke,NULL,NULL,NULL,NULL,NULL,0,NULL);
    require(w->child!=NULL,"release-tail child after Close"); ++w->releases;
}
static DWORD WINAPI queue_pending_worker(void* data)
{ require(xl6ContinuationWaitRunOne(((QueueWork*)data)->queue),"waiting dispatcher consumed exactly one ticket"); return 0; }
static void pending_queue_invocation(void)
{
    Fixture f; xmemdebugsnapshot before; QueueWork w={0};
    begin_fixture(&f,&before); w.fixture=&f;
    w.queue=xl6ContinuationQueueCreate(1); w.promise=xrtPromiseCreate(&w.future,NULL);
    require(w.queue && w.promise,"pending queue fixtures");
    require(xl6ContinuationReserveOwned(w.queue,&w.ticket,queue_pending_run,queue_pending_release,&w,
        &f.owner.descriptor.base)==XL6_CONTINUATION_ADMITTED && xl6ContinuationTraceBind(&w.ticket,no_edges),"pending queue reserve");
    xl6ContinuationCommit(&w.ticket); xl6ContinuationReady(&w.ticket); xl6ContinuationTransportReleased(&w.ticket);
    InterlockedExchange(&f.owner.closed,1);
    HANDLE thread=CreateThread(NULL,0,queue_pending_worker,&w,0,NULL); require(thread!=NULL,"pending dispatcher");
    await_flag(&w.ready); bool observed=false; DWORD started=GetTickCount();
    while(!observed && (DWORD)(GetTickCount()-started)<5000) {
        xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)) {
            xrtownershipref future=xrtFutureOwnership(w.future),lease=xl6CodeLeaseOwnership(w.ticket.codeLease),
                ticket=xl6ContinuationOwnership(&w.ticket); size_t refs=0,count=0,unused=0;
            observed=future.Ops->Count(future.Data,&refs) && refs==3 && lease.Ops->Count(lease.Data,&count) && count==2 &&
                !ticket.Ops->Count(ticket.Data,&unused) && xrtFutureState(w.future)==XFUTURE_PENDING;
            require(xrtOwnershipScopeEnd(&freeze),"end parked continuation view");
        }
        SwitchToThread();
    }
    require(xrtPromiseResolve(w.promise,NULL),"resolve continuation wait"); await_flag(&w.releaseGate.entered);
    for(unsigned i=0;i<100;++i) {
        xrtownershipscope freeze={0};
        xrtownershipref ticket=xl6ContinuationOwnership(&w.ticket),lease=xl6CodeLeaseOwnership(w.ticket.codeLease);
        size_t unused=177,count=0;
        require(xrtOwnershipFreezeTryBegin(&freeze),"waiting release does not hold global mutation admission");
        require(!ticket.Ops->Count(ticket.Data,&unused) && unused==177 &&
            lease.Ops->Count(lease.Data,&count) && count==2 &&
            xl6ContinuationQueueActivationCount(w.queue)==1 && !xl6ContinuationQueueIsIdle(w.queue),
            "detached release frame rejects inspection while real code roots and queue credit survive");
        require(xrtOwnershipScopeEnd(&freeze),"end release-tail observation");
    }
    InterlockedExchange(&w.releaseGate.proceed,1); join(thread);
    require(observed && w.runs==1 && w.releases==1 && w.child && xl6ContinuationQueueIsIdle(w.queue) &&
        read_flag(&f.owner.pins)==2 && read_flag(&f.owner.acquired)==2,"pending run and inherited release-tail ownership");
    closures->Release(w.child); require(read_flag(&f.owner.pins)==1,"child last release ends only its inherited pin");
    xl6ContinuationQueueClose(w.queue); require(xl6ContinuationQueueDestroy(w.queue),"pending queue cleanup");
    xrtPromiseDestroy(w.promise); xrtFutureDestroy(w.future); end_fixture(&f,&before);
}
int main(int argc, char** argv)
{
    xallocator allocator={NULL,allocate,reallocate,deallocate};
    require(xrtSetAllocator(&allocator) && xrtMemDebugEnable(true),"tracked gated allocator");
    closures=xl6ClosureRuntimeV1Get(); classes=xl6ClassRuntimeV1Get();
    if(argc==2 && !strcmp(argv[1],"--factory-baseline")) { callback_or_factory(5); return 0; }
    if(argc==2 && !strcmp(argv[1],"--queue-baseline")) { frozen_queue_transition(0); return 0; }
    require(argc==1,"arguments");
    for(unsigned round=0;round<10;++round) {
        for(unsigned kind=0;kind<10;++kind) frozen_transition(kind);
        for(unsigned kind=0;kind<10;++kind) callback_or_factory(kind);
        waiting_invocation();
        for(unsigned kind=0;kind<10;++kind) frozen_queue_transition(kind);
        pending_queue_invocation();
    }
    puts("Resident ownership scopes: 100 frozen lease/stack/closure transitions, 50 in-flight callback tails, 50 allocator-held factory transfers, 10 real pending invocations; exact graph/count/allocator balance, no native root registration; not module collection");
    puts("Continuation ownership scopes: 100 frozen queue/transport transitions, 10 actual pending dispatches, 10 physical retirement tails with after-Close child escape; exact credits/code roots/allocator balance; not module collection");
    return 0;
}
