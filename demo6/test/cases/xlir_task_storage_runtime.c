#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_TASK_POOL
#include "lib/xrt/xrt.h"
#include "xlir_task_storage_drop.h"
#include "src/runtime/xl6_closure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
static const XL6_TaskRuntimeV1* api;
static LONG flag(volatile LONG* p) { return InterlockedCompareExchange(p,0,0); }
static void await_flag(volatile LONG* p)
{ ULONGLONG end=GetTickCount64()+10000;while(!flag(p)){assert(GetTickCount64()<end);SwitchToThread();} }
static void freeze_begin(xrtownershipscope* scope)
{ ULONGLONG end=GetTickCount64()+1000;while(!xrtOwnershipFreezeTryBegin(scope)){assert(GetTickCount64()<end);SwitchToThread();} }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
typedef struct Hooks { volatile LONG entered,leave,notified,released; unsigned runs,drops,outcome; bool holdRun,holdDrop,selfJoin; XL6_TaskPool* pool; } Hooks;
typedef struct Payload { Hooks* hooks; xvalue* value; } Payload;
static void hold(Hooks* hooks) { InterlockedExchange(&hooks->entered,1);await_flag(&hooks->leave); }
static bool empty_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ (void)data;(void)visit;(void)context;return true; }
static bool payload_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ return visit(xrtValueOwnership(((const Payload*)data)->value),context); }
static XL6_TaskOutcome run(struct xcancel* cancel,void* data,XL6_TaskResult* result)
{
    Payload* payload=data;(void)cancel;++payload->hooks->runs;
    if(payload->hooks->holdRun)hold(payload->hooks);
    if(payload->hooks->outcome){
        xrtSetErrorKind(payload->hooks->outcome==1?XERR_STATE:payload->hooks->outcome==2?XERR_CANCELLED:XERR_CLOSED);
        return XL6_TASK_FAILED;
    }
    *result=(XL6_TaskResult){0};return XL6_TASK_SUCCESS;
}
static void drop(void* data,void* unused)
{
    Payload* payload=data;Hooks* hooks=payload->hooks;(void)unused;
    if(hooks->holdDrop)hold(hooks);
    if(hooks->selfJoin){assert(!api->PoolQuiesce(hooks->pool) && api->PoolIsClosing(hooks->pool));xrtClearError();}
    xrtValueRelease(payload->value);++hooks->drops;
}
static Payload* create(Hooks* hooks)
{
    Payload* payload=api->Allocate(NULL,sizeof(*payload),run,drop,payload_trace);assert(payload);
    payload->hooks=hooks;payload->value=xrtValueInt(173);assert(payload->value);return payload;
}
static XL6_TaskPool* pool(void)
{ XL6_TaskPoolConfig config={1,4,0};XL6_TaskPool* p=api->PoolCreate(&config);assert(p);return p; }
static void finish(XL6_TaskPool* p)
{ assert(api->PoolQuiesce(p) && api->PoolIsQuiescent(p) && api->PoolIsClosing(p) && api->PoolDestroy(p)); }
static bool visits(xrtownershipref child,void* context)
{ (void)child;++*(unsigned*)context;return true; }
static void refuse(xrtownershipref root)
{ size_t count=987;unsigned n=0;assert(!root.Ops->Count(root.Data,&count) && count==987);
    assert(!root.Ops->Trace(root.Data,visits,&n) && n==0); }
static void notify(void* data)
{
    Hooks* hooks=data;assert(!api->PoolQuiesce(hooks->pool));xrtClearError();
    InterlockedExchange(&hooks->notified,1);await_flag(&hooks->released);
}
static void notify_release(void* data) { (void)data; }
typedef struct Joining { XL6_TaskPool* pool; volatile LONG entered,done; } Joining;
static DWORD WINAPI joining(void* data)
{
    Joining* join=data;InterlockedExchange(&join->entered,1);
    assert(api->PoolQuiesce(join->pool));InterlockedExchange(&join->done,1);return 0;
}
static void queued_and_active(void)
{
    for(unsigned n=0;n<100;++n)for(unsigned language=0;language<2;++language){
        xmemdebugsnapshot before;xrtownershipscope freeze={0};Hooks blocked={0},queued={0};
        XL6_TaskPool* p;Payload *first,*second;xfuture *a,*b;xrtownershipref root;size_t count=0;
        xrtMemDebugSnapshot(&before);p=pool();blocked.holdRun=true;blocked.pool=queued.pool=p;
        first=create(&blocked);a=api->Submit(p,first,NULL,NULL,false);assert(a);await_flag(&blocked.entered);
        second=create(&queued);root=api->Ownership(second);
        /* Full physical inspection now follows the actual Future -> Job edge.
         * Select the resident data/result contract; keep legacy submission
         * coverage in the other paths and the active blocker above. */
        b=xl6TaskSubmitPayloadV1(p,second,NULL,XL6_FUTURE_PAYLOAD_VALUE,language!=0);assert(b);
        freeze_begin(&freeze);refuse(api->Ownership(first));
        assert(root.Ops->Count(root.Data,&count) && count==(language?3u:1u));
        /* A readable queued frame reaches its real, still active pool. */
        { xrtownershipresult graph;assert(!xrtOwnershipInspect(&root,1,NULL,0,&graph));xrtClearError(); }
        assert(xrtOwnershipScopeEnd(&freeze));InterlockedExchange(&blocked.leave,1);
        assert(xrtFutureWait(a)==XWAIT_OK && xrtFutureWait(b)==XWAIT_OK);xrtFutureDestroy(a);xrtFutureDestroy(b);finish(p);
        assert(blocked.runs==1 && blocked.drops==1 && queued.runs==1 && queued.drops==1);balanced(&before);
    }
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before;Hooks hooks={0};xrtownershipscope freeze={0};XL6_TaskPool* p;Payload* data;xfuture* future;
        xrtMemDebugSnapshot(&before);p=pool();hooks.holdDrop=true;hooks.pool=p;data=create(&hooks);
        future=api->Submit(p,data,NULL,NULL,true);assert(future);await_flag(&hooks.entered);
        freeze_begin(&freeze);refuse(api->Ownership(data));assert(xrtOwnershipScopeEnd(&freeze));
        InterlockedExchange(&hooks.leave,1);assert(xrtFutureWait(future)==XWAIT_OK);xrtFutureDestroy(future);finish(p);
        assert(hooks.runs==1 && hooks.drops==1);balanced(&before);
    }
}
static bool native_slot(xrtownershipref ref,void* data)
{xrtownershipref* native=data;assert(!native->Data);*native=ref;return true;}
static void complete_queued_graphs(void)
{
    for(unsigned n=0;n<100;++n)for(unsigned language=0;language<2;++language){
        xmemdebugsnapshot before;Hooks warm={0},queued={0};xrtMemDebugSnapshot(&before);
        XL6_TaskPool* p=pool();warm.pool=queued.pool=p;
        xfuture* first=api->Submit(p,create(&warm),NULL,NULL,false);assert(first&&xrtFutureWait(first)==XWAIT_OK);xrtFutureDestroy(first);
        xrtownershipscope freeze={0};xrtownershipref view=xl6TaskPoolOwnership(p);ULONGLONG deadline=GetTickCount64()+5000;
        for(;;){
            freeze_begin(&freeze);size_t count;xrtownershipref native={0};
            if(view.Ops->Count(view.Data,&count)&&view.Ops->Trace(view.Data,native_slot,&native)&&
                native.Data&&native.Ops->Count(native.Data,&count))break;
            assert(xrtOwnershipScopeEnd(&freeze)&&GetTickCount64()<deadline);SwitchToThread();
        }
        /* Parked workers stabilize the complete graph, without an opaque
         * active blocker or truncated pool traversal. */
        Payload* data=create(&queued);xrtownershipref root=api->Ownership(data);
        xfuture* output=xl6TaskSubmitPayloadV1(p,data,NULL,XL6_FUTURE_PAYLOAD_VALUE,language!=0);assert(output);
        xrtownershipresult graph;assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
        if(!n)printf("Complete task graph language=%u nodes=%llu edges=%llu roots=%llu\n",language,
            (unsigned long long)graph.NodeCount,(unsigned long long)graph.EdgeCount,(unsigned long long)graph.ExternalRootCount);
        assert(graph.NodeCount==(language?9u:7u)&&graph.EdgeCount==(language?15u:9u)&&graph.ExternalRootCount==2);
        assert(xrtOwnershipScopeEnd(&freeze));
        assert(xrtFutureWait(output)==XWAIT_OK);xrtFutureDestroy(output);finish(p);
        assert(warm.runs==1&&warm.drops==1&&queued.runs==1&&queued.drops==1);balanced(&before);
    }
    puts("Task pool inspection: 200 complete parked-worker graphs plus 200 independently retained active-pool refusals");
}
static void notifications(void)
{
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before;Hooks hooks={0};xrtownershipscope freeze={0};XL6_TaskPool* p;Payload* data;xfuture* future;xfuturewatch watch;
        Joining join={0};HANDLE thread;
        xrtMemDebugSnapshot(&before);p=pool();hooks.holdRun=true;hooks.pool=p;hooks.outcome=n%4;data=create(&hooks);
        future=api->Submit(p,data,NULL,NULL,true);assert(future);await_flag(&hooks.entered);
        assert(xrtFutureWatchInitPhased(&watch,notify,notify_release,&hooks,empty_trace) && xrtFutureWatchAdd(future,&watch)==XFUTURE_WATCH_PENDING);
        InterlockedExchange(&hooks.leave,1);await_flag(&hooks.notified);
        /* XRT drains forwarding notifications iteratively. The task frame can
         * already be gone here: inspect the retained Future, never that stale
         * borrowed frame address. The real pool worker must still be joined. */
        freeze_begin(&freeze);
        { xrtownershipref root=xrtFutureOwnership(future);xrtownershipresult graph;
            assert(xrtOwnershipInspect(&root,1,NULL,0,&graph) && graph.NodeCount>0 && graph.ExternalRootCount>0); }
        assert(xrtOwnershipScopeEnd(&freeze));
        join.pool=p;thread=CreateThread(NULL,0,joining,&join,0,NULL);assert(thread);await_flag(&join.entered);
        Sleep(2);assert(!flag(&join.done));
        InterlockedExchange(&hooks.released,1);assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);
        assert(flag(&join.done) && api->PoolIsQuiescent(p));xrtFutureDestroy(future);finish(p);
        assert(hooks.runs==1 && hooks.drops==1);balanced(&before);
    }
}
static void synchronous_cancel(void)
{
    for(unsigned n=0;n<100;++n)for(unsigned language=0;language<2;++language){
        xmemdebugsnapshot before;Hooks hooks={0};XL6_TaskPool* p;xfuture* future;xcancel* cancel;
        xrtMemDebugSnapshot(&before);p=pool();hooks.selfJoin=true;hooks.pool=p;cancel=xrtCancelCreate();assert(cancel && xrtCancelRequest(cancel));
        future=api->Submit(p,create(&hooks),cancel,NULL,language!=0);
        assert(future && hooks.runs==0 && hooks.drops==1 && xrtFutureState(future)==XFUTURE_CANCELLED);
        xrtFutureDestroy(future);xrtCancelDestroy(cancel);finish(p);balanced(&before);
    }
}
static unsigned allocation_faults(void)
{
    unsigned faults=0;
    for(unsigned n=0;n<20;++n)for(unsigned language=0;language<2;++language){
        unsigned budget;
        for(budget=0;budget<128;++budget){
            xmemdebugsnapshot before;Hooks hooks={0};XL6_TaskPool* p;Payload* data;xfuture* future;bool triggered;
            xrtMemDebugSnapshot(&before);p=pool();hooks.pool=p;data=create(&hooks);
            assert(xrtMemDebugFailAfter(budget));future=api->Submit(p,data,NULL,NULL,language!=0);
            triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            if(triggered){assert(!future && xrtGetError() && !hooks.runs && !hooks.drops);++faults;
                xrtClearError();refuse(api->Ownership(data));api->Discard(data);}
            else {assert(future && xrtFutureWait(future)==XWAIT_OK);xrtFutureDestroy(future);}
            finish(p);assert(hooks.drops==1);balanced(&before);if(!triggered)break;
        }
        assert(budget<128);
    }
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before;Hooks hooks={0};Payload* data;
        xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(0));
        assert(!api->Allocate(NULL,sizeof(Payload),run,drop,payload_trace) && xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();xrtClearError();data=create(&hooks);assert(xrtMemDebugFailAfter(0));api->Discard(data);
        assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();assert(hooks.drops==1);balanced(&before);
    }
    return faults;
}
static void immediate_retirement_races(void)
{
    xmemdebugsnapshot before;XL6_TaskPool* p;xrtMemDebugSnapshot(&before);p=pool();
    for(unsigned n=0;n<2000;++n){
        Hooks hooks={0};xfuture* future;hooks.pool=p;
        future=api->Submit(p,create(&hooks),NULL,NULL,(n&1)!=0);assert(future);
        assert(xrtFutureWait(future)==XWAIT_OK && hooks.runs==1 && hooks.drops==1);xrtFutureDestroy(future);
    }
    finish(p);balanced(&before);
    puts("Task submission races: 2000 immediate job/submitter/Watch retirements, exact allocator balance");
}
typedef struct DllOwner {
    XL6_CodeOwnerGraph descriptor;TaskStorageHooks hooks;XL6_Closure* child;
    unsigned pins,acquired,released,unmapped;bool closed;HMODULE library;void* address;
    volatile LONG entered,leave;XL6_TaskPool* pool;
} DllOwner;
static int acquire(void* data) { DllOwner* o=data;if(o->closed)return 0;++o->pins;++o->acquired;return 1; }
static void release(void* data)
{
    DllOwner* o=data;MEMORY_BASIC_INFORMATION memory;assert(o->pins);--o->pins;++o->released;
    if(!o->pins){assert(o->closed && o->hooks.returned==1 && o->hooks.drops==1);
        assert(FreeLibrary(o->library) && VirtualQuery(o->address,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_FREE);++o->unmapped;}
}
static bool owner_count(const void* data,size_t* count) { const DllOwner* o=data;*count=o->pins+!o->closed;return true; }
static const xrtownershipops owner_ops={owner_count,empty_trace};
static void child_call(void) {}
static void during(void* data,int dropPhase)
{
    DllOwner* o=data;if(!dropPhase){InterlockedExchange(&o->entered,1);await_flag(&o->leave);}
    assert(o->closed && o->pins==1 && !o->released);
    assert(!api->PoolQuiesce(o->pool));xrtClearError();
    XL6_Closure* child=xl6ClosureRuntimeV1Get()->Create(&o->descriptor.base,child_call,NULL,NULL,NULL,NULL,NULL,0,empty_trace);assert(child);
    if(!o->child)o->child=child;else xl6ClosureRuntimeV1Get()->Release(child);
}
static void dll_lifetimes(const char* library)
{
    for(unsigned n=0;n<50;++n)for(unsigned cancel=0;cancel<2;++cancel){
        xmemdebugsnapshot before;DllOwner o={0};XL6_TaskRun runFn;XL6_TaskDrop dropFn;XL6_CodeContextTrace traceFn;TaskStoragePayload* data;
        xfuture* future;xcancel* token=NULL;
        xrtMemDebugSnapshot(&before);o.library=LoadLibraryA(library);assert(o.library);
        { FARPROC p=GetProcAddress(o.library,"task_storage_run");memcpy(&runFn,&p,sizeof(runFn));o.address=(void*)p;
            p=GetProcAddress(o.library,"task_storage_drop");memcpy(&dropFn,&p,sizeof(dropFn));
            p=GetProcAddress(o.library,"task_storage_trace");memcpy(&traceFn,&p,sizeof(traceFn)); }
        assert(runFn && dropFn && traceFn);o.descriptor=(XL6_CodeOwnerGraph){{sizeof(o.descriptor),&o,acquire,release},0x584C36434F574E31ULL,&owner_ops};
        o.pool=pool();o.hooks=(TaskStorageHooks){during,&o,0,0,0};
        data=api->Allocate(&o.descriptor.base,sizeof(*data),runFn,dropFn,traceFn);assert(data && o.pins==1);data->hooks=&o.hooks;
        if(cancel){o.closed=true;token=xrtCancelCreate();assert(token && xrtCancelRequest(token));}
        future=api->Submit(o.pool,data,token,NULL,true);assert(future);
        if(!cancel){await_flag(&o.entered);o.closed=true;
            assert(!api->Allocate(&o.descriptor.base,sizeof(*data),runFn,dropFn,traceFn) && xrtGetError());xrtClearError();InterlockedExchange(&o.leave,1);}
        assert(xrtFutureWait(future)==XWAIT_OK && xrtFutureState(future)==(cancel?XFUTURE_CANCELLED:XFUTURE_RESOLVED));
        xrtFutureDestroy(future);xrtCancelDestroy(token);finish(o.pool);
        assert(o.hooks.runs==!cancel && o.hooks.drops==1 && o.hooks.returned==1 && o.child && o.pins==1 && o.acquired==1 && !o.released);
        { XL6_ClosureScope scope={0};xl6ClosureRuntimeV1Get()->Enter(&scope,o.child);o.child->invoke();xl6ClosureRuntimeV1Get()->Leave(&scope); }
        xl6ClosureRuntimeV1Get()->Release(o.child);assert(o.released==1 && o.unmapped==1);balanced(&before);
    }
}
int main(int argc,char** argv)
{
    xmemdebugsnapshot before,after;unsigned faults;assert(argc==2 && xrtMemDebugEnable(true));api=xl6TaskRuntimeV1Get();
    xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    queued_and_active();complete_queued_graphs();notifications();synchronous_cancel();faults=allocation_faults();immediate_retirement_races();dll_lifetimes(argv[1]);balanced(&before);xrtMemDebugSnapshot(&after);
    assert(faults>=200);
    puts("Resident tasks: 200 queued physical graphs, 200 executing refusals, 100 blocked-drop refusals, 100 frozen notification graphs and callback-tail joins across four outcomes, 200 synchronous cancellation self-join refusals, 100 allocation rollbacks, 100 armed-OOM discards, 100 actual DLL lifetimes with after-Close child closures; no native root registration");
    printf("Task storage allocator: %u true pre-submit faults, %llu real allocations, zero live delta\n",faults,(unsigned long long)(after.AllocCount-before.AllocCount));
    return 0;
}
