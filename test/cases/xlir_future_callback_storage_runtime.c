#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "xlir_future_callback_storage_drop.h"
#include "src/runtime/xl6_closure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
static const XL6_FutureCallbackRuntimeV1* api;
static LONG flag(volatile LONG* p) { return InterlockedCompareExchange(p,0,0); }
/* TCC's WinAPI declarations expose GetTickCount, not GetTickCount64.
 * Unsigned elapsed time is wrap-safe for these one/ten-second bounds. */
static void await_flag(volatile LONG* p)
{ DWORD began=GetTickCount();while(!flag(p)){assert((DWORD)(GetTickCount()-began)<10000);SwitchToThread();} }
static void freeze_begin(xrtownershipscope* scope)
{ DWORD began=GetTickCount();while(!xrtOwnershipFreezeTryBegin(scope)){assert((DWORD)(GetTickCount()-began)<1000);SwitchToThread();} }
static void join(HANDLE thread) { assert(thread && WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && CloseHandle(thread)); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount && before->UseAfterFreeCount==after.UseAfterFreeCount);
}
typedef struct Hooks { volatile LONG entered,leave;unsigned calls,drops;bool holdProc,holdDrop; } Hooks;
typedef struct Payload { Hooks* hooks;xfuture* source;xvalue* value; } Payload;
static bool empty_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ (void)data;(void)visit;(void)context;return true; }
static bool trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ const Payload* p=data;return visit(xrtFutureOwnership(p->source),context) && visit(xrtValueOwnership(p->value),context); }
static void hold(Hooks* h) { InterlockedExchange(&h->entered,1);await_flag(&h->leave); }
static void proc(const xfutureresult* input,xpromise* output,xcancel* cancel,void* data)
{
    Payload* p=data;assert(input);++p->hooks->calls;
    if(p->hooks->holdProc)hold(p->hooks);
    if(xrtCancelRequested(cancel)){assert(xrtPromiseCancel(output));return;}
    assert(xrtPromiseForward(output,p->source));
}
static void drop(void* data,void* unused)
{ Payload* p=data;(void)unused;if(p->hooks->holdDrop)hold(p->hooks);xrtFutureDestroy(p->source);xrtValueRelease(p->value);++p->hooks->drops; }
static Payload* create(xfuture* source,Hooks* hooks)
{
    Payload* p=api->Allocate(NULL,sizeof(*p),proc,drop,trace);assert(p);
    p->hooks=hooks;p->source=xrtFutureRef(source);p->value=xrtValueInt(174);assert(p->source && p->value);return p;
}
static bool visit(xrtownershipref child,void* context) { (void)child;++*(unsigned*)context;return true; }
static void refuse(xrtownershipref root)
{ size_t count=765;unsigned n=0;assert(!root.Ops->Count(root.Data,&count) && count==765);assert(!root.Ops->Trace(root.Data,visit,&n) && !n); }
static void complete(xpromise* promise,unsigned outcome)
{
    if(outcome==0)assert(xrtPromiseResolve(promise,NULL));
    else if(outcome==2)assert(xrtPromiseCancel(promise));
    else if(outcome==3)assert(xrtPromiseClose(promise));
    else{xrtSetErrorKind(XERR_STATE);assert(xrtPromiseReject(promise,xrtGetError()));xrtClearError();}
}
typedef struct Completion { xpromise* promise;unsigned outcome;volatile LONG entered,leave;bool gated; } Completion;
static DWORD WINAPI completing(void* data)
{ Completion* c=data;InterlockedExchange(&c->entered,1);if(c->gated)await_flag(&c->leave);complete(c->promise,c->outcome);return 0; }
static void pending_and_ready(void)
{
    for(unsigned n=0;n<100;++n)for(unsigned ready=0;ready<2;++ready){
        xmemdebugsnapshot before;Hooks h={0};xfuture *source=NULL,*output;Payload* p;size_t refs=0;xrtownershipscope freeze={0};
        xrtMemDebugSnapshot(&before);xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        p=create(source,&h);refuse(api->Ownership(p));if(ready)complete(promise,n%4);
        output=api->Start(p,source);assert(output && !xrtGetError());
        if(!ready){
            xrtownershipref root=xrtFutureOwnership(source);xrtownershipresult graph;
            freeze_begin(&freeze);assert(api->Ownership(p).Ops->Count(api->Ownership(p).Data,&refs) && refs==2);
            assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
            if(n==0)printf("Future callback graph: nodes=%llu edges=%llu roots=%llu\n",(unsigned long long)graph.NodeCount,(unsigned long long)graph.EdgeCount,(unsigned long long)graph.ExternalRootCount);
            /* Source/output Future nodes are the two external roots. Promise
             * and Future handles share a node, not separate root identities. */
            assert(graph.NodeCount==6 && graph.EdgeCount==9 && graph.ExternalRootCount==2);
            assert(xrtOwnershipScopeEnd(&freeze));complete(promise,n%4);
        }
        assert(xrtFutureState(output)==(n%4==0?XFUTURE_RESOLVED:n%4==1?XFUTURE_FAILED:n%4==2?XFUTURE_CANCELLED:XFUTURE_CLOSED));
        assert(h.calls==1 && h.drops==1);xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    }
}
static void active_and_cancelled(void)
{
    for(unsigned n=0;n<100;++n)for(unsigned phase=0;phase<2;++phase){
        xmemdebugsnapshot before;Hooks h={0};Completion completion={0};xfuture *source=NULL,*output;Payload* p;xrtownershipscope freeze={0};
        xrtMemDebugSnapshot(&before);completion.promise=xrtPromiseCreate(&source,NULL);assert(completion.promise);
        h.holdProc=!phase;h.holdDrop=!!phase;p=create(source,&h);output=api->Start(p,source);assert(output);
        HANDLE thread=CreateThread(NULL,0,completing,&completion,0,NULL);assert(thread);await_flag(&h.entered);
        freeze_begin(&freeze);refuse(api->Ownership(p));assert(xrtOwnershipScopeEnd(&freeze));InterlockedExchange(&h.leave,1);join(thread);
        assert(h.calls==1 && h.drops==1 && xrtFutureState(output)==XFUTURE_RESOLVED);
        xrtPromiseDestroy(completion.promise);xrtFutureDestroy(source);xrtFutureDestroy(output);balanced(&before);
    }
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before;Hooks h={0};xfuture *source=NULL,*output;
        xrtMemDebugSnapshot(&before);xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        output=api->Start(create(source,&h),source);assert(output && xrtFutureCancel(output));
        assert(!h.calls && !h.drops);complete(promise,0);assert(h.calls==1 && h.drops==1 && xrtFutureState(output)==XFUTURE_CANCELLED);
        xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    }
}
static unsigned allocation_faults(void)
{
    unsigned faults=0;
    for(unsigned n=0;n<20;++n)for(unsigned ready=0;ready<2;++ready)for(unsigned budget=0;budget<128;++budget){
        xmemdebugsnapshot before;Hooks h={0};xfuture *source=NULL,*output;Payload* p;
        xrtMemDebugSnapshot(&before);xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);p=create(source,&h);if(ready)complete(promise,0);
        assert(xrtMemDebugFailAfter(budget));output=api->Start(p,source);bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!output){assert(failed && xrtGetError() && !h.calls && !h.drops);++faults;api->Discard(p);assert(h.drops==1);xrtClearError();}
        else{assert(!failed);if(!ready)complete(promise,0);assert(h.calls==1 && h.drops==1);xrtFutureDestroy(output);}
        xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);if(!failed)break;assert(budget!=127);
    }
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before;Hooks h={0};xfuture* source=NULL;
        xrtMemDebugSnapshot(&before);xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        assert(xrtMemDebugFailAfter(0) && !api->Allocate(NULL,sizeof(Payload),proc,drop,trace) && xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtClearError();
        Payload* p=create(source,&h);assert(xrtMemDebugFailAfter(0));api->Discard(p);assert(!xrtMemDebugFailTriggered() && h.drops==1);xrtMemDebugFailClear();
        xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    }
    return faults;
}
static void registration_races(void)
{
    for(unsigned n=0;n<1000;++n){
        xmemdebugsnapshot before;Hooks h={0};Completion c={0};xfuture *source=NULL,*output;
        xrtMemDebugSnapshot(&before);c.promise=xrtPromiseCreate(&source,NULL);assert(c.promise);c.outcome=n%4;c.gated=true;
        Payload* p=create(source,&h);HANDLE thread=CreateThread(NULL,0,completing,&c,0,NULL);assert(thread);await_flag(&c.entered);InterlockedExchange(&c.leave,1);
        output=api->Start(p,source);assert(output);join(thread);assert(h.calls==1 && h.drops==1);
        xrtPromiseDestroy(c.promise);xrtFutureDestroy(source);xrtFutureDestroy(output);balanced(&before);
    }
}
typedef struct DllOwner {
    XL6_CodeOwnerGraph descriptor;FutureCallbackStorageHooks hooks;XL6_Closure* closure;
    unsigned pins,acquired,released,unmapped;bool closed;HMODULE library;void* address;
    XL6_FutureCallbackProc childProc;XL6_FutureCallbackDrop drop;XL6_CodeContextTrace trace;void(*child)(void);
    xfuture *childSource,*childOutput;xpromise* childPromise;
} DllOwner;
static int owner_acquire(void* data) { DllOwner* o=data;if(o->closed)return 0;++o->pins;++o->acquired;return 1; }
static void owner_release(void* data)
{
    DllOwner* o=data;MEMORY_BASIC_INFORMATION memory;assert(o->pins);--o->pins;++o->released;
    if(!o->pins){assert(o->closed && o->hooks.calls==1 && o->hooks.children==1 && o->hooks.drops==2 && o->hooks.returned==2 && o->hooks.childCalls==1);
        assert(FreeLibrary(o->library) && VirtualQuery(o->address,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_FREE);++o->unmapped;}
}
static bool owner_count(const void* data,size_t* count) { const DllOwner* o=data;*count=o->pins+!o->closed;return true; }
static const xrtownershipops owner_ops={owner_count,empty_trace};
static void forward(xpromise* output,xfuture* source) { assert(xrtPromiseForward(output,source)); }
static void during(void* data,int phase)
{
    DllOwner* o=data;assert(o->closed && o->pins==1 && !o->released);
    if(!phase){
        o->closure=xl6ClosureRuntimeV1Get()->Create(&o->descriptor.base,o->child,NULL,NULL,NULL,NULL,NULL,0,empty_trace);assert(o->closure);
        o->childPromise=xrtPromiseCreate(&o->childSource,NULL);assert(o->childPromise);
        FutureCallbackStorageData* p=api->Allocate(&o->descriptor.base,sizeof(*p),o->childProc,o->drop,o->trace);assert(p);p->hooks=&o->hooks;
        o->childOutput=api->Start(p,o->childSource);assert(o->childOutput && !xrtFutureDone(o->childOutput));
    }
}
static void dll_lifetimes(const char* library)
{
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before;DllOwner o={0};XL6_FutureCallbackProc parent;xfuture *source=NULL,*output;
        xrtMemDebugSnapshot(&before);o.library=LoadLibraryA(library);assert(o.library);
        { FARPROC p=GetProcAddress(o.library,"future_callback_storage_proc");memcpy(&parent,&p,sizeof(parent));o.address=(void*)p;
            p=GetProcAddress(o.library,"future_callback_storage_child_proc");memcpy(&o.childProc,&p,sizeof(o.childProc));
            p=GetProcAddress(o.library,"future_callback_storage_drop");memcpy(&o.drop,&p,sizeof(o.drop));
            p=GetProcAddress(o.library,"future_callback_storage_trace");memcpy(&o.trace,&p,sizeof(o.trace));
            p=GetProcAddress(o.library,"future_callback_storage_child");memcpy(&o.child,&p,sizeof(o.child)); }
        assert(parent && o.childProc && o.drop && o.trace && o.child);
        o.descriptor=(XL6_CodeOwnerGraph){{sizeof(o.descriptor),&o,owner_acquire,owner_release},0x584C36434F574E31ULL,&owner_ops};
        xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);o.hooks=(FutureCallbackStorageHooks){during,forward,&o,source,0,0,0,0,0};
        FutureCallbackStorageData* data=api->Allocate(&o.descriptor.base,sizeof(*data),parent,o.drop,o.trace);assert(data && o.pins==1);data->hooks=&o.hooks;
        output=api->Start(data,source);assert(output);o.closed=true;
        assert(!api->Allocate(&o.descriptor.base,sizeof(*data),parent,o.drop,o.trace) && xrtGetError());xrtClearError();complete(promise,0);
        assert(o.hooks.calls==1 && o.hooks.drops==1 && o.hooks.returned==1 && !o.hooks.children && o.pins==1 && !o.released);
        { XL6_ClosureScope scope={0};xl6ClosureRuntimeV1Get()->Enter(&scope,o.closure);o.closure->invoke();xl6ClosureRuntimeV1Get()->Leave(&scope); }
        assert(o.hooks.childCalls==1);xl6ClosureRuntimeV1Get()->Release(o.closure);assert(!o.released);
        complete(o.childPromise,0);assert(o.released==1 && o.unmapped==1 && o.acquired==1 && xrtFutureState(o.childOutput)==XFUTURE_RESOLVED);
        xrtPromiseDestroy(o.childPromise);xrtFutureDestroy(o.childSource);xrtFutureDestroy(o.childOutput);
        xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    }
}
int main(int argc,char** argv)
{
    xmemdebugsnapshot before,after;unsigned faults;assert(argc==2 && xrtMemDebugEnable(true));api=xl6FutureCallbackRuntimeV1Get();
    xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    pending_and_ready();active_and_cancelled();faults=allocation_faults();registration_races();dll_lifetimes(argv[1]);balanced(&before);xrtMemDebugSnapshot(&after);
    assert(faults>0);
    puts("Resident Future callbacks: 100 pending graphs, 100 ready outcomes, 200 active/drop inspection refusals, 100 reverse cancellations, 100 allocation rollbacks, 100 armed-OOM discards, 1000 registration races, 100 real DLL parent/child continuations and closures after Close; no native root registration");
    printf("Future callback allocator: %u true preparation faults, %llu real allocations, zero live delta\n",faults,(unsigned long long)(after.AllocCount-before.AllocCount));return 0;
}
