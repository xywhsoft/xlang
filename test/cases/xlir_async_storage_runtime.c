#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_VALUE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_frame_storage.h"
#include "src/runtime/xl6_continuation_queue.h"
#include "src/runtime/xl6_slot.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static const XL6_FrameStorageRuntimeV1* storage;
static LONG flag(volatile LONG* p) { return InterlockedCompareExchange(p,0,0); }
static void await_flag(volatile LONG* p)
{
    DWORD started=GetTickCount();
    while(!flag(p)) { assert((DWORD)(GetTickCount()-started)<10000); SwitchToThread(); }
}
static void join(HANDLE thread)
{
    DWORD result;
    assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && GetExitCodeThread(thread,&result) && !result);
    assert(CloseHandle(thread));
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
typedef struct Worker { void* frame; unsigned mode; volatile LONG ready,go,attempt,done; } Worker;
static XL6_ContinuationQueue* generatedQueue;
static xfuture* sources[2];
static volatile LONG generatedEntered;
extern xfuture* async_storage_generated_create(void);
extern void async_storage_generated_prepare(void), async_storage_generated_finish(void);
XL6_ContinuationQueue* async_storage_queue(void) { return generatedQueue; }
xfuture* async_storage_source(int64_t stage) { assert(stage==0 || stage==1); return xrtFutureRef(sources[stage]); }
void async_storage_entered(void) { InterlockedIncrement(&generatedEntered); }
static DWORD WINAPI frozen_worker(void* data)
{
    Worker* w=data; xl6_error_scope warm={0};
    assert(xl6ErrorScopeBegin(&warm) && xl6ErrorScopeEnd(&warm).ok);
    InterlockedExchange(&w->ready,1); await_flag(&w->go);
    { xrtownershipscope refused={0}; assert(!xrtOwnershipFreezeTryBegin(&refused)); }
    InterlockedExchange(&w->attempt,1);
    if(w->mode==0) w->frame=storage->Allocate(128);
    else if(w->mode==1) storage->Free(w->frame);
    else w->frame=async_storage_generated_create();
    InterlockedExchange(&w->done,1); return 0;
}
static void free_number(void* value,void* data) { (void)data; xrtFree(value); }
static void resolve_source(xpromise* promise)
{
    int64_t* value=xrtMalloc(sizeof(*value)); assert(value); *value=86;
    assert(xrtPromiseResolveOwned(promise,value,free_number,NULL));
}
static void frozen_storage(unsigned mode,unsigned rounds)
{
    for(unsigned n=0;n<rounds;++n) {
        xmemdebugsnapshot before; xrtownershipscope freeze={0}; Worker w={0}; xpromise* promises[2]={0};
        xrtMemDebugSnapshot(&before); w.mode=mode; generatedEntered=0;
        if(mode==1) { w.frame=storage->Allocate(128); assert(w.frame); memset(w.frame,172,128); }
        if(mode==2) {
            generatedQueue=xl6ContinuationQueueCreate(1); assert(generatedQueue);
            for(unsigned i=0;i<2;++i) { promises[i]=xrtPromiseCreate(&sources[i],NULL); assert(promises[i]); }
        }
        HANDLE thread=CreateThread(NULL,0,frozen_worker,&w,0,NULL); assert(thread); await_flag(&w.ready);
        assert(xrtOwnershipFreezeTryBegin(&freeze)); InterlockedExchange(&w.go,1); await_flag(&w.attempt);
        Sleep(2);
        if(flag(&w.done)||flag(&generatedEntered)) fprintf(stderr,"async physical storage escaped freeze, mode=%u\n",mode);
        assert(!flag(&w.done) && !flag(&generatedEntered));
        if(mode==1) for(unsigned i=0;i<128;++i) assert(((unsigned char*)w.frame)[i]==172);
        assert(xrtOwnershipScopeEnd(&freeze)); join(thread);
        if(mode==0) { assert(w.frame); storage->Free(w.frame); }
        if(mode==2) {
            xfuture* output=w.frame; assert(output && !generatedEntered);
            assert(xl6ContinuationRunOne(generatedQueue)); /* ready entry -> first pending await */
            assert(generatedEntered==1 && xrtFutureState(output)==XFUTURE_PENDING);
            xl6ContinuationQueueClose(generatedQueue);
            for(unsigned i=0;i<2;++i) { resolve_source(promises[i]); assert(xl6ContinuationRunOne(generatedQueue)); }
            assert(xrtFutureState(output)==XFUTURE_RESOLVED && *(int64_t*)xrtFutureValue(output)==172);
            assert(xl6ContinuationQueueIsIdle(generatedQueue) && xl6ContinuationQueueDestroy(generatedQueue)); generatedQueue=NULL;
            for(unsigned i=0;i<2;++i) { xrtPromiseDestroy(promises[i]); xrtFutureDestroy(sources[i]); sources[i]=NULL; }
            xrtFutureDestroy(output);
        }
        balance(&before);
    }
}

typedef struct Owner { XL6_CodeOwnerGraph descriptor; unsigned pins,acquires,releases; bool closed; } Owner;
static int acquire(void* data) { Owner* o=data; if(o->closed)return 0; ++o->pins; ++o->acquires; return 1; }
static void release_owner(void* data) { Owner* o=data; assert(o->pins==1); --o->pins; ++o->releases; }
static bool owner_count(const void* data,size_t* count)
{ const Owner* o=data; *count=o->pins+(o->closed?0u:1u); return true; }
static bool no_edges(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data; (void)visit; (void)context; return true; }
static const xrtownershipops owner_ops={owner_count,no_edges};
typedef struct Hooks {
    XL6_ContinuationQueue* queue; Owner owner;
    volatile LONG entered,leave,freeAttempt,parentReturned,successorRan,waiterReady;
    struct Frame* successor;
} Hooks;
typedef struct Frame { XL6_ContinuationTicket ticket; xvalue* value; Hooks* hooks; bool parent; } Frame;
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{ return visit(xrtValueOwnership(((const Frame*)data)->value),context); }
static void frame_drop(void* data);
static void frame_run(void* data)
{
    Frame* frame=data; Hooks* h=frame->hooks;
    if(frame->parent) {
        Frame* next=storage->Allocate(sizeof(*next)); assert(next); *next=(Frame){0}; next->hooks=h;
        assert(xl6ContinuationReserveSuccessor(&frame->ticket,&next->ticket,frame_run,frame_drop,next)==XL6_CONTINUATION_ADMITTED);
        xvalue* empty=NULL;
        XL6_SlotWrite writes[]={{&next->value,&frame->value,sizeof(frame->value)},{&frame->value,&empty,sizeof(empty)}};
        xl6SlotRuntimeV1Get()->Apply(writes,2);
        assert(xl6ContinuationTraceBind(&next->ticket,trace)); h->successor=next;
        xl6ContinuationCommit(&next->ticket); xl6ContinuationReady(&next->ticket); xl6ContinuationTransportReleased(&next->ticket);
    } else {
        int64 value=0; assert(flag(&h->parentReturned));
        assert(xrtValueGetInt(frame->value,&value) && value==172); InterlockedIncrement(&h->successorRan);
    }
}
static void frame_drop(void* data)
{
    Frame* frame=data; Hooks* h=frame->hooks; bool parent=frame->parent;
    xvalue* value;
    if(parent) {
        InterlockedExchange(&h->entered,1); await_flag(&h->leave);
        /* Empty parent has no payload cleanup to obscure the physical Free
         * boundary. The exact successor owns its original transferred value. */
        assert(!frame->value); InterlockedExchange(&h->freeAttempt,1);
    } else {
        xvalue* empty=NULL; XL6_SlotWrite writes[]={{&value,&frame->value,sizeof(value)},{&frame->value,&empty,sizeof(empty)}};
        xl6SlotRuntimeV1Get()->Apply(writes,2); xrtValueRelease(value);
    }
    storage->Free(frame);
    if(parent) InterlockedExchange(&h->parentReturned,1);
}
static DWORD WINAPI parent_worker(void* data)
{ assert(xl6ContinuationRunOne(((Hooks*)data)->queue)); return 0; }
static DWORD WINAPI successor_worker(void* data)
{
    Hooks* h=data; InterlockedExchange(&h->waiterReady,1);
    assert(xl6ContinuationWaitRunOne(h->queue)); return 0;
}
static bool reject_visit(xrtownershipref ref,void* data) { (void)ref; ++*(unsigned*)data; return true; }
static void rejected(xrtownershipref root)
{
    size_t sentinel=199; unsigned visits=0; xrtownershipresult initial={11,22,33,44},result=initial; bool live=true;
    assert(!root.Ops->Count(root.Data,&sentinel) && sentinel==199);
    assert(!root.Ops->Trace(root.Data,reject_visit,&visits) && !visits);
    assert(!xrtOwnershipInspectReachable(&root,1,NULL,0,&live,&result,NULL,NULL));
    assert(live && !memcmp(&initial,&result,sizeof(result))); xrtClearError();
}
static void retirement(unsigned rounds)
{
    for(unsigned n=0;n<rounds;++n) {
        xmemdebugsnapshot before; Hooks h={0}; xrtownershipscope freeze={0};
        xrtMemDebugSnapshot(&before); h.queue=xl6ContinuationQueueCreate(1); assert(h.queue);
        h.owner.descriptor=(XL6_CodeOwnerGraph){{sizeof(XL6_CodeOwnerGraph),&h.owner,acquire,release_owner},0x584C36434F574E31ULL,&owner_ops};
        Frame* frame=storage->Allocate(sizeof(*frame)); assert(frame); *frame=(Frame){0};
        frame->value=xrtValueInt(172); frame->hooks=&h; frame->parent=true; assert(frame->value);
        assert(xl6ContinuationReserveOwned(h.queue,&frame->ticket,frame_run,frame_drop,frame,&h.owner.descriptor.base)==XL6_CONTINUATION_ADMITTED);
        assert(xl6ContinuationTraceBind(&frame->ticket,trace));
        xl6ContinuationCommit(&frame->ticket); xl6ContinuationReady(&frame->ticket); xl6ContinuationTransportReleased(&frame->ticket);
        h.owner.closed=true; xl6ContinuationQueueClose(h.queue);
        xrtownershipref parent=xl6ContinuationOwnership(&frame->ticket),lease=xl6CodeLeaseOwnership(frame->ticket.codeLease);
        xrtownershipresult graph;
        assert(xrtOwnershipInspect(&parent,1,NULL,0,&graph) && graph.NodeCount==4 && graph.EdgeCount==3 && graph.ExternalRootCount==1);
        HANDLE first=CreateThread(NULL,0,parent_worker,&h,0,NULL); assert(first); await_flag(&h.entered);
        HANDLE second=CreateThread(NULL,0,successor_worker,&h,0,NULL); assert(second); await_flag(&h.waiterReady);
        /* A newly started waiter can briefly own dequeue admission. Wait for
         * that bounded transition, while cleanup remains held indefinitely by
         * this fixture. The old release-wide admission never becomes free. */
        DWORD started=GetTickCount(); bool frozen=false;
        while(!(frozen=xrtOwnershipFreezeTryBegin(&freeze)) && (DWORD)(GetTickCount()-started)<1000) SwitchToThread();
        if(!frozen) { fprintf(stderr,"async release callback held mutation admission\n"); assert(false); }
        xrtownershipref next=xl6ContinuationOwnership(&h.successor->ticket); size_t refs=0;
        rejected(parent); rejected(next);
        assert(lease.Ops->Count(lease.Data,&refs) && refs==2 && h.owner.pins==1);
        assert(!xl6ContinuationQueueIsIdle(h.queue) && xl6ContinuationQueueActivationCount(h.queue)==1 && !xl6ContinuationQueueDestroy(h.queue));
        InterlockedExchange(&h.leave,1); await_flag(&h.freeAttempt); Sleep(2);
        assert(!flag(&h.parentReturned) && !flag(&h.successorRan)); rejected(parent); rejected(next);
        assert(xrtOwnershipScopeEnd(&freeze)); join(first); join(second);
        assert(flag(&h.parentReturned)==1 && flag(&h.successorRan)==1 && !h.owner.pins && h.owner.acquires==1 && h.owner.releases==1);
        assert(xl6ContinuationQueueIsIdle(h.queue) && xl6ContinuationQueueDestroy(h.queue)); balance(&before);
    }
}
static void oom(void)
{
    for(unsigned n=0;n<100;++n) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(0)); assert(!storage->Allocate(128) && xrtMemDebugFailTriggered());
        xrtMemDebugFailClear(); balance(&before);
        void* frame=storage->Allocate(128); assert(frame); assert(xrtMemDebugFailAfter(0)); storage->Free(frame);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); balance(&before);
    }
}
int main(int argc,char** argv)
{
    xmemdebugsnapshot before,after;
    assert(xrtMemDebugEnable(true) && xrtMemDebugEnabled()); storage=xl6FrameStorageRuntimeV1Get();
    assert(storage && storage->Allocate && storage->Free);
    if(argc==2 && !strcmp(argv[1],"--generated-only")) {
        xrtMemDebugSnapshot(&before); async_storage_generated_prepare(); frozen_storage(2,1);
        async_storage_generated_finish(); balance(&before); return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--retirement-only")) { retirement(1); return 0; }
    assert(argc==1); xrtMemDebugSnapshot(&before);
    async_storage_generated_prepare();
    frozen_storage(0,100); frozen_storage(1,100); frozen_storage(2,100); retirement(100); oom();
    async_storage_generated_finish();
    balance(&before); xrtMemDebugSnapshot(&after); assert(after.AllocCount>before.AllocCount);
    puts("Async physical storage: 200 frozen allocate/free transitions, 100 generated launches, 200 generated Watch retirements, 100 blocked-cleanup successor handoffs, 100 true OOM rollbacks, 100 armed-OOM frees; exact graph/count/credit/allocator balance, no native root registration");
    printf("Async storage allocator: %llu real allocations, zero live delta\n",(unsigned long long)(after.AllocCount-before.AllocCount));
    return 0;
}
