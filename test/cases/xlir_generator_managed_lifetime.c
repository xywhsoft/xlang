#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#include "lib/xrt/xrt.h"
#include "src/api/xl_foreign_error.h"
#include "xlir_generator_managed_dll.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_lifetime_platform.h"

static const XL6_GeneratorRuntimeV1* api;
static const XL6_GeneratorRuntimeV2* managed;
static unsigned invocations,payloadDrops,dllLifetimes,probes,ooms,validationRefusals;
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static XLIR_TEST_THREAD_RESULT XLIR_TEST_THREAD_CALL freeze_thread(void* unused)
{
    (void)unused;xrtownershipscope scope={0};uint64_t stop=xlirTestMilliseconds()+5000;
    while(!xrtOwnershipFreezeTryBegin(&scope)){assert(xlirTestMilliseconds()<stop);xlirTestYield();}
    assert(xrtOwnershipScopeEnd(&scope));return 0;
}
static void cross_thread(void)
{
    XLIR_TestThread thread; assert(xlirTestThreadStart(&thread,freeze_thread,NULL));
    xlirTestThreadJoin(thread);++probes;
}
static bool empty_trace(const void* data,xrtownershipvisitor visit,void* context)
{(void)data;(void)visit;(void)context;return true;}
static void empty_drop(void* data){assert(!data);}
/* Stats is externally owned for this transaction; it owns no graph edges. */
static void borrowed_stats_drop(void* data){assert(data);}
static void legacy_drop(void* frame){api->FrameFree(frame);}
typedef struct Stats {unsigned mode,finallyCount,dropped;XL6_Generator* generator;XL6_Generator* extra;void* frame;void* extraFrame;} Stats;
typedef struct Payload {Stats* stats;xvalue* object;unsigned char initialized;} Payload;
static const XL6_ActivationCaptureSlot slots[]={
    {{XL6_CAPTURE_TRIVIAL,offsetof(Payload,stats),sizeof(Stats*)},SIZE_MAX},
    {{XL6_CAPTURE_XVALUE,offsetof(Payload,object),sizeof(xvalue*)},offsetof(Payload,initialized)}};
static const size_t cleanup[]={1};
static const XL6_ActivationCaptureLayoutV1 layout={sizeof(layout),sizeof(Payload),2,slots,1,cleanup};
static const XL6_CaptureSlot itemSlot={XL6_CAPTURE_TRIVIAL,0,sizeof(int64_t)};
static const XL6_CaptureLayoutV1 itemLayout={sizeof(itemLayout),sizeof(int64_t),1,&itemSlot};
static void finalizer(xvalue* object,void* data)
{
    (void)object;Stats* stats=data;Payload* frame=stats->frame;
    assert(!frame->object&&!frame->initialized&&stats->finallyCount==1&&!stats->dropped);++stats->dropped;++payloadDrops;
    cross_thread();
    /* Payload cleanup is active but its actual owner may create and drop
     * separate physical references. It cannot free the executing frame. */
    assert(managed->FrameRetain(frame)==frame);managed->FrameRelease(frame);
    assert(managed->Retain(stats->generator)==stats->generator);api->Destroy(stats->generator);
    if(stats->mode==2){stats->extraFrame=managed->FrameRetain(frame);assert(stats->extraFrame);}
}
static bool finalizer_checked(xvalue* object,void* data){finalizer(object,data);return true;}
static const xvalueobjectownershipv1 policy={sizeof(policy),finalizer,empty_trace,borrowed_stats_drop,empty_trace,empty_drop,finalizer_checked};
static bool resume(XL6_Generator* generator)
{
    Payload* frame=generator->frame;Stats* stats=frame->stats;
    if(!generator->closing&&!generator->state){
        int64_t item=23;api->SetState(generator,1);
        if(stats->mode==1)api->Destroy(generator); /* Release the caller's actual owner during Next. */
        return api->Put(generator,&item,sizeof(item));
    }
    ++stats->finallyCount;++invocations;cross_thread();
    if(stats->mode==0){stats->extra=managed->Retain(generator);assert(stats->extra);}
    api->Finish(generator);return false;
}
static void reentrant(unsigned mode)
{
    Stats stats={0};stats.mode=mode;xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Payload* frame=managed->FrameAllocate(sizeof(*frame),&layout);assert(frame);*frame=(Payload){&stats,NULL,0};stats.frame=frame;
    frame->object=xrtValueObject();
    assert(frame->object&&xrtValueObjectLifetimeBindOwned(frame->object,NULL,empty_trace,empty_drop)&&
        xrtValueObjectFinalizerPrepareOwned(frame->object,finalizer,&stats,empty_trace,borrowed_stats_drop)&&
        xrtValueObjectOwnershipBindV1(frame->object,&policy)&&xrtValueObjectFinalizerCommit(frame->object));
    frame->initialized=1;
    XL6_Generator* generator=managed->Create(NULL,frame,resume,&itemLayout);assert(generator);stats.generator=generator;
    XL6_GeneratorStep step=api->Next(generator);assert(step.ok&&step.value);
    if(mode!=1)api->Destroy(generator);
    assert(stats.finallyCount==1&&stats.dropped==1);
    if(mode==0){
        assert(stats.extra==generator);size_t count=0;xrtownershipref ref=api->Ownership(stats.extra);
        assert(ref.Ops->Count(ref.Data,&count)&&count==1);
        xl6_error_result closed=api->Close(stats.extra);assert(closed.ok&&!closed.error&&stats.finallyCount==1);api->Destroy(stats.extra);
    }
    if(mode==2){
        size_t count=0;xrtownershipref ref=managed->FrameOwnership(stats.extraFrame);
        assert(ref.Ops->Count(ref.Data,&count)&&count==1);managed->FrameRelease(stats.extraFrame);
    }
    balance(&before);
}
static void invalid_and_oom(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_ActivationCaptureLayoutV1 invalid=layout;invalid.cleanupCount=0;
    assert(!managed->FrameAllocate(sizeof(Payload),&invalid));++validationRefusals;xrtClearError();
    Payload* frame=managed->FrameAllocate(sizeof(*frame),&layout);assert(frame);*frame=(Payload){0};
    assert(!managed->FrameRetain(frame));++validationRefusals;xrtClearError();
    xrtownershipref ref=managed->FrameOwnership(frame);size_t count=313;
    assert(!ref.Ops->Count(ref.Data,&count)&&count==313);
    assert(!api->Create(NULL,frame,resume,legacy_drop,sizeof(int64_t),NULL,empty_trace,empty_trace));++validationRefusals;xrtClearError();
    XL6_CaptureLayoutV1 badItem=itemLayout;badItem.count=0;
    assert(!managed->Create(NULL,frame,resume,&badItem));++validationRefusals;xrtClearError();
    assert(xrtMemDebugFailAfter(0));assert(!managed->Create(NULL,frame,resume,&itemLayout)&&xrtMemDebugFailTriggered());
    ++ooms;xrtMemDebugFailClear();xrtClearError();managed->FrameDiscard(frame);
    void* old=api->FrameAllocate(sizeof(Payload));assert(old);
    assert(!managed->Create(NULL,old,resume,&itemLayout));++validationRefusals;xrtClearError();api->FrameFree(old);
    assert(xrtMemDebugFailAfter(0));assert(!managed->FrameAllocate(sizeof(Payload),&layout)&&xrtMemDebugFailTriggered());
    ++ooms;xrtMemDebugFailClear();balance(&before);
}
typedef struct DllOwner {
    XL6_CodeOwnerGraph descriptor;ManagedDllHooks hooks;XLIR_TestLibrary library;void* address;
    XL6_Generator* generator;xrtownershipref frameRef;unsigned pins,acquired,released,unmapped;bool closed;
} DllOwner;
static int owner_acquire(void* data)
{DllOwner* owner=data;if(owner->closed)return 0;++owner->pins;++owner->acquired;return 1;}
static void owner_release(void* data)
{
    DllOwner* owner=data;assert(owner->pins);--owner->pins;++owner->released;
    if(!owner->pins){
        assert(owner->closed&&owner->hooks.entered==1&&owner->hooks.returned==1);
        xlirTestLibraryCloseUnmapped(owner->library,owner->address);++owner->unmapped;
    }
}
static bool owner_count(const void* data,size_t* count)
{const DllOwner* owner=data;*count=owner->pins+!owner->closed;return true;}
static const xrtownershipops ownerOps={owner_count,empty_trace};
static void dll_during(void* data)
{
    DllOwner* owner=data;assert(owner->closed&&owner->pins==1&&!owner->released);cross_thread();
    xrtownershipscope scope={0};assert(xrtOwnershipFreezeTryBegin(&scope));size_t count=911;
    xrtownershipref root=api->Ownership(owner->generator);
    assert(!root.Ops->Count(root.Data,&count)&&count==911&&!owner->frameRef.Ops->Count(owner->frameRef.Data,&count));
    assert(xrtOwnershipScopeEnd(&scope));
    XL6_CodeLease* child=NULL;assert(xl6CodeLeaseAcquireScoped(&owner->descriptor.base,&child)==XL6_CODE_LEASE_ACQUIRED&&child);
    xl6CodeLeaseRelease(child);assert(!owner->released);
}
static void dll_lifetime(const char* library,bool frameLast,bool currentAbi)
{
    DllOwner owner={0};XL6_GeneratorResume callback;const XL6_ActivationCaptureLayoutV1* (*frameLayout)(void);
    const XL6_CaptureLayoutV1* (*yieldLayout)(void);
    const XL6_ActivationCaptureLayoutV2* (*frameLayoutV2)(void);
    const XL6_CaptureLayoutV2* (*yieldLayoutV2)(void);
    const XL6_GeneratorRuntimeV3* resident=xl6GeneratorRuntimeV3Get();
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    owner.library=xlirTestLibraryOpen(library);assert(owner.library);
    void* p=xlirTestLibrarySymbol(owner.library,"generator_managed_resume");memcpy(&callback,&p,sizeof(callback));owner.address=p;
    p=xlirTestLibrarySymbol(owner.library,"generator_managed_frame_layout");memcpy(&frameLayout,&p,sizeof(frameLayout));
    p=xlirTestLibrarySymbol(owner.library,"generator_managed_item_layout");memcpy(&yieldLayout,&p,sizeof(yieldLayout));assert(callback&&frameLayout&&yieldLayout);
    p=xlirTestLibrarySymbol(owner.library,"generator_managed_frame_layout_v3");memcpy(&frameLayoutV2,&p,sizeof(frameLayoutV2));
    p=xlirTestLibrarySymbol(owner.library,"generator_managed_item_layout_v3");memcpy(&yieldLayoutV2,&p,sizeof(yieldLayoutV2));assert(frameLayoutV2&&yieldLayoutV2);
    owner.descriptor=(XL6_CodeOwnerGraph){{sizeof(owner.descriptor),&owner,owner_acquire,owner_release},0x584C36434F574E31ULL,&ownerOps};
    owner.hooks=(ManagedDllHooks){api,dll_during,&owner,0,0};
    ManagedDllFrame* frame=currentAbi ? resident->FrameAllocate(sizeof(*frame),frameLayoutV2()) :
        managed->FrameAllocate(sizeof(*frame),frameLayout());assert(frame);frame->hooks=&owner.hooks;
    XL6_Generator* generator=currentAbi ? resident->Create(&owner.descriptor.base,frame,callback,yieldLayoutV2()) :
        managed->Create(&owner.descriptor.base,frame,callback,yieldLayout());assert(generator&&owner.pins==1);owner.generator=generator;
    owner.frameRef=managed->FrameOwnership(frame);xrtownershipref root=api->Ownership(generator);
    xrtownershipscope scope={0};assert(xrtOwnershipFreezeTryBegin(&scope));size_t count=0;
    const xrtownershippreparationv1* prep=NULL;const xrtownershipadapterv1* control=xl6GeneratorOwnershipAdapterV1(root,&prep);
    assert(control&&prep&&control->Hold(root.Data)&&root.Ops->Count(root.Data,&count)&&count==2);
    const xrtownershipadapterv1* frameAdapter=xl6GeneratorOwnershipAdapterV1(owner.frameRef,&prep);
    assert(frameAdapter&&prep&&frameAdapter->Hold(owner.frameRef.Data)&&owner.frameRef.Ops->Count(owner.frameRef.Data,&count)&&count==2);
    assert(xrtOwnershipScopeEnd(&scope));
    assert(managed->Retain(generator)==generator);control->Drop(root.Data);
    assert(root.Ops->Count(root.Data,&count)&&count==2);
    XL6_GeneratorStep step=api->Next(generator);assert(step.ok&&step.value);owner.closed=true;
    xl6_error_result closed=api->Close(generator);assert(closed.ok&&!closed.error&&owner.hooks.returned==1);
    api->Destroy(generator);assert(!owner.unmapped&&!owner.released&&root.Ops->Count(root.Data,&count)&&count==1);
    if(frameLast){
        api->Destroy(generator);assert(!owner.unmapped&&!owner.released);
        assert(owner.frameRef.Ops->Count(owner.frameRef.Data,&count)&&count==1);
        frameAdapter->Drop(owner.frameRef.Data);
    }else{
        frameAdapter->Drop(owner.frameRef.Data);assert(!owner.unmapped&&!owner.released);
        assert(root.Ops->Count(root.Data,&count)&&count==1);api->Destroy(generator);
    }
    assert(owner.unmapped==1&&owner.acquired==1&&owner.released==1);++dllLifetimes;balance(&before);
}
int main(int argc,char** argv)
{
    assert(argc==2&&xrtMemDebugEnable(true));api=xl6GeneratorRuntimeV1Get();managed=xl6GeneratorRuntimeV2Get();
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    for(unsigned n=0;n<50;++n){for(unsigned mode=0;mode<3;++mode)reentrant(mode);invalid_and_oom();dll_lifetime(argv[1],false,false);dll_lifetime(argv[1],true,false);}
    assert(invocations==150&&payloadDrops==150&&dllLifetimes==100&&probes==400&&ooms==100&&validationRefusals==250);
    puts("Managed generator lifetimes: 150 last-owner reentrant closes, 150 sparse payload drops, 100 actual DLL unloads with separately held control/frame descriptors, 400 cross-thread freezes, 100 real OOM failures, 250 ABI refusals; exact memory balance");
    for(unsigned n=0;n<50;++n){dll_lifetime(argv[1],false,true);dll_lifetime(argv[1],true,true);}
    assert(dllLifetimes==200&&probes==500&&invocations==150&&payloadDrops==150&&ooms==100&&validationRefusals==250);
    puts("Current V3 descriptor lifetimes: 100 actual callback library unloads, independent control/frame last owners, 100 cross-thread freezes and exact memory balance");return 0;
}
