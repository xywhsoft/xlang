#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "xlir_generator_storage_drop.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

extern void* generator_storage_create(void);
extern int generator_storage_next(void*);
extern int64_t generator_storage_get(void*);
extern xrtownershipref generator_storage_ownership(const void*);
extern void generator_storage_destroy(void*);
static const XL6_GeneratorRuntimeV1* api;
static volatile LONG generatedEntered;
void generator_storage_entered(void) { InterlockedIncrement(&generatedEntered); }
static LONG flag(volatile LONG* value) { return InterlockedCompareExchange(value,0,0); }
static void wait_flag(volatile LONG* value)
{
    ULONGLONG deadline=GetTickCount64()+10000;
    while(!flag(value)) { assert(GetTickCount64()<deadline); SwitchToThread(); }
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
typedef struct FrozenWorker { void* generator; bool create; volatile LONG ready,go,attempt,done; int result; } FrozenWorker;
static DWORD WINAPI frozen_worker(void* data)
{
    FrozenWorker* worker=data; xl6_error_scope warm={0};
    assert(xl6ErrorScopeBegin(&warm)); assert(xl6ErrorScopeEnd(&warm).ok);
    InterlockedExchange(&worker->ready,1); wait_flag(&worker->go);
    { xrtownershipscope refused={0}; assert(!xrtOwnershipFreezeTryBegin(&refused)); }
    InterlockedExchange(&worker->attempt,1);
    if(worker->create) worker->generator=generator_storage_create();
    else worker->result=generator_storage_next(worker->generator);
    InterlockedExchange(&worker->done,1); return 0;
}
static void generated_freezes(unsigned rounds)
{
    for(unsigned n=0;n<rounds;++n)for(unsigned create=0;create<2;++create) {
        xmemdebugsnapshot before; xrtownershipscope freeze={0}; FrozenWorker worker={0}; HANDLE thread;
        xrtMemDebugSnapshot(&before); worker.create=create!=0; generatedEntered=0;
        if(!create) { worker.generator=generator_storage_create(); assert(worker.generator); }
        thread=CreateThread(NULL,0,frozen_worker,&worker,0,NULL); assert(thread); wait_flag(&worker.ready);
        assert(xrtOwnershipFreezeTryBegin(&freeze)); InterlockedExchange(&worker.go,1); wait_flag(&worker.attempt);
        Sleep(2);
        if(flag(&generatedEntered)||flag(&worker.done)) fprintf(stderr,"generated generator lifecycle escaped freeze\n");
        assert(!flag(&generatedEntered) && !flag(&worker.done));
        if(!create) { xrtownershipref root=generator_storage_ownership(worker.generator); xrtownershipresult result;
            assert(xrtOwnershipInspect(&root,1,NULL,0,&result) && result.NodeCount==2 && result.EdgeCount==1 && result.ExternalRootCount==1); }
        assert(xrtOwnershipScopeEnd(&freeze)); assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0); CloseHandle(thread);
        assert(worker.generator);
        if(create) assert(generator_storage_next(worker.generator)==1); else assert(worker.result==1);
        assert(generatedEntered==1 && generator_storage_get(worker.generator)==171);
        assert(generator_storage_next(worker.generator)==0); generator_storage_destroy(worker.generator); balanced(&before);
    }
}
typedef struct ActiveHooks { volatile LONG entered,leave; bool holdResume,holdDrop; unsigned dropped; } ActiveHooks;
typedef struct ActiveFrame { xvalue* value; ActiveHooks* hooks; } ActiveFrame;
static void held(ActiveHooks* hooks)
{ InterlockedExchange(&hooks->entered,1); wait_flag(&hooks->leave); }
static bool active_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ return visit(xrtValueOwnership(((const ActiveFrame*)data)->value),context); }
static bool item_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ return visit(xrtValueOwnership(*(xvalue*const*)data),context); }
static bool empty_trace(const void* data,XL6_ModuleOwnershipVisit visit,void* context)
{ (void)data; (void)visit; (void)context; return true; }
static void check_value(xvalue* value)
{ int64 number=0; assert(xrtValueGetInt(value,&number) && number==171); }
static void item_drop(void* data) { xrtValueRelease(*(xvalue**)data); }
static void active_drop(void* data)
{
    ActiveFrame* frame=data; ActiveHooks* hooks=frame->hooks;
    if(hooks->holdDrop) held(hooks);
    xrtValueRelease(frame->value); ++hooks->dropped; api->FrameFree(frame);
}
static bool active_resume(XL6_Generator* generator)
{
    ActiveFrame* frame=generator->frame;
    if(generator->closing || generator->state) { api->Finish(generator); return false; }
    if(frame->hooks->holdResume) held(frame->hooks);
    { xvalue* value=xrtValueRetain(frame->value); assert(api->Put(generator,&value,sizeof(value))); }
    api->SetState(generator,1); return true;
}
static XL6_Generator* active_create(ActiveHooks* hooks)
{
    ActiveFrame* frame=api->FrameAllocate(sizeof(*frame)); XL6_Generator* generator;
    assert(frame); frame->value=xrtValueInt(171); frame->hooks=hooks; assert(frame->value);
    generator=api->Create(NULL,frame,active_resume,active_drop,sizeof(xvalue*),item_drop,active_trace,item_trace);
    assert(generator); return generator;
}
static bool capture_frame(xrtownershipref child,void* data) { *(xrtownershipref*)data=child; return true; }
typedef struct ActiveWorker { XL6_Generator* generator; bool destroy; } ActiveWorker;
static DWORD WINAPI active_worker(void* data)
{
    ActiveWorker* worker=data;
    if(worker->destroy) api->Destroy(worker->generator);
    else { XL6_GeneratorStep step=api->Next(worker->generator); assert(step.ok && step.value); }
    return 0;
}
static void active_windows(void)
{
    for(unsigned n=0;n<100;++n)for(unsigned drop=0;drop<2;++drop) {
        xmemdebugsnapshot before; ActiveHooks hooks={0}; XL6_Generator* generator; xrtownershipref root,frame={0};
        ActiveWorker worker; HANDLE thread; xrtownershipscope freeze={0}; size_t count=987;
        xrtMemDebugSnapshot(&before); hooks.holdResume=!drop; hooks.holdDrop=drop!=0;
        generator=active_create(&hooks); root=api->Ownership(generator);
        assert(root.Ops->Trace(root.Data,capture_frame,&frame) && frame.Data);
        { xrtownershipresult graph; assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
            assert(graph.NodeCount==3 && graph.EdgeCount==2 && graph.ExternalRootCount==1); }
        worker=(ActiveWorker){generator,drop!=0}; thread=CreateThread(NULL,0,active_worker,&worker,0,NULL); assert(thread);
        wait_flag(&hooks.entered);
        /* The source callback is allowed to wait without pinning mutation. Both
         * public root and independently discovered physical frame refuse. */
        assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(!root.Ops->Count(root.Data,&count) && count==987);
        assert(!frame.Ops->Count(frame.Data,&count) && count==987);
        assert(xrtOwnershipScopeEnd(&freeze)); InterlockedExchange(&hooks.leave,1);
        assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0); CloseHandle(thread);
        if(!drop) {
            xvalue* item=NULL; assert(api->ItemGet(generator,&item,sizeof(item),true)); check_value(item);
            xrtValueRelease(item); api->Destroy(generator);
        }
        assert(hooks.dropped==1); balanced(&before);
    }
}
static void allocation_and_transfer(void)
{
    for(unsigned n=0;n<100;++n) {
        xmemdebugsnapshot before; void* frame; ActiveHooks hooks={0}; XL6_Generator* generator;
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(0)); frame=api->FrameAllocate(sizeof(ActiveFrame));
        assert(!frame && xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); balanced(&before);
        xrtMemDebugSnapshot(&before); frame=api->FrameAllocate(1); assert(frame);
        assert(xrtMemDebugFailAfter(0)); generator=api->Create(NULL,frame,active_resume,active_drop,sizeof(int),NULL,empty_trace,empty_trace);
        assert(!generator && xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError(); api->FrameFree(frame); balanced(&before);
        xrtMemDebugSnapshot(&before); generator=active_create(&hooks);
        assert(xrtMemDebugFailAfter(0));
        { XL6_GeneratorStep step=api->Next(generator); xvalue* item=NULL; assert(step.ok && step.value);
            assert(api->ItemGet(generator,&item,sizeof(item),true)); check_value(item); xrtValueRelease(item); }
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); api->Destroy(generator); assert(hooks.dropped==1); balanced(&before);
    }
}
typedef struct DllOwner {
    XL6_CodeOwnerGraph descriptor; GeneratorStorageHooks hooks; XL6_CodeLease* child;
    unsigned pins,acquired,released,unmapped; bool closed; HMODULE library; void* address;
} DllOwner;
static int owner_acquire(void* data)
{ DllOwner* owner=data; if(owner->closed)return 0; ++owner->pins; ++owner->acquired; return 1; }
static void owner_release(void* data)
{
    DllOwner* owner=data; MEMORY_BASIC_INFORMATION memory;
    assert(owner->pins); --owner->pins; ++owner->released;
    if(!owner->pins) {
        assert(owner->closed && owner->hooks.returned==1 && owner->hooks.dropped==1);
        assert(FreeLibrary(owner->library)); assert(VirtualQuery(owner->address,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_FREE);
        ++owner->unmapped;
    }
}
static bool owner_count(const void* data,size_t* count)
{ const DllOwner* owner=data; *count=owner->pins+!owner->closed; return true; }
static const xrtownershipops owner_ops={owner_count,empty_trace};
static void dll_during(void* data)
{
    DllOwner* owner=data; XL6_CodeLease* child=NULL;
    assert(owner->closed && owner->pins==1 && owner->released==0);
    assert(xl6CodeLeaseAcquireScoped(&owner->descriptor.base,&child)==XL6_CODE_LEASE_ACQUIRED && child);
    if(!owner->child) owner->child=child; else { assert(owner->child==child); xl6CodeLeaseRelease(child); }
}
static void dll_lifetimes(const char* library)
{
    for(unsigned n=0;n<50;++n)for(unsigned resume=0;resume<2;++resume) {
        DllOwner owner={0}; XL6_GeneratorResume next; XL6_GeneratorFree drop; XL6_CodeContextTrace trace;
        GeneratorStorageFrame* frame; XL6_Generator* generator; xmemdebugsnapshot before;
        owner.library=LoadLibraryA(library); assert(owner.library);
        { FARPROC p=GetProcAddress(owner.library,"generator_storage_resume"); memcpy(&next,&p,sizeof(next)); owner.address=(void*)p;
            p=GetProcAddress(owner.library,"generator_storage_frame_drop"); memcpy(&drop,&p,sizeof(drop));
            p=GetProcAddress(owner.library,"generator_storage_trace"); memcpy(&trace,&p,sizeof(trace)); }
        assert(next && drop && trace); owner.descriptor=(XL6_CodeOwnerGraph){{sizeof(owner.descriptor),&owner,owner_acquire,owner_release},0x584C36434F574E31ULL,&owner_ops};
        owner.hooks.api=api; owner.hooks.during=dll_during; owner.hooks.context=&owner;
        xrtMemDebugSnapshot(&before); frame=api->FrameAllocate(sizeof(*frame)); assert(frame); frame->hooks=&owner.hooks;
        generator=api->Create(&owner.descriptor.base,frame,next,drop,sizeof(int),NULL,trace,trace); assert(generator && owner.pins==1);
        owner.closed=true;
        { xrtownershipref root=api->Ownership(generator); xrtownershipresult graph;
            assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)); assert(graph.NodeCount==4 && graph.EdgeCount==3 && graph.ExternalRootCount==1); }
        if(resume) { XL6_GeneratorStep step=api->Next(generator); assert(step.ok && !step.value); }
        api->Destroy(generator);
        assert(owner.hooks.resumed==resume && owner.hooks.returned==1 && owner.child && !owner.unmapped && !owner.released);
        xl6CodeLeaseRelease(owner.child); assert(owner.acquired==1 && owner.released==1 && owner.unmapped==1); balanced(&before);
    }
}
int main(int argc,char** argv)
{
    xl6_error_scope warm={0}; xmemdebugsnapshot initial,after;
    assert(xrtMemDebugEnable(true) && xrtMemDebugEnabled());
    api=xl6GeneratorRuntimeV1Get(); assert(xl6ErrorScopeBegin(&warm)); assert(xl6ErrorScopeEnd(&warm).ok);
    xrtMemDebugSnapshot(&initial);
    if(argc==2 && !strcmp(argv[1],"--generated-only")) { generated_freezes(1); return 0; }
    assert(argc==2); generated_freezes(100); active_windows(); allocation_and_transfer(); dll_lifetimes(argv[1]);
    balanced(&initial); xrtMemDebugSnapshot(&after); assert(after.AllocCount>initial.AllocCount);
    puts("Resident generators: 200 generated freezes, 200 active control/frame refusals, 200 real OOM rollbacks, 100 armed-OOM transfers, 100 actual DLL lifetimes with after-Close child leases; exact graphs and allocator balance, no native root registration");
    printf("Generator storage allocator: %llu real allocations, zero live delta\n",(unsigned long long)(after.AllocCount-initial.AllocCount));
    return 0;
}
