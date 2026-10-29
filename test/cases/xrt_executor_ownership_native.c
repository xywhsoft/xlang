/* Ordinary offline native prerequisite, not a claim of public language API. */
#define XRT_MODULE_EXECUTOR
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_WAIT
#ifdef XL6_EXECUTOR_TEST_RUNTIME
#define XRUNTIME_IMPLEMENTATION
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#else
#define XRT_IMPLEMENTATION
#include "lib/xrt/xrt.h"
#endif
#include "release/include/xl_clock.h"
#include <assert.h>
#include <stdio.h>

typedef struct Item { size_t references; xatomic32* completed; } Item;
typedef struct Slots { xrtownershipref items[2]; size_t count; } Slots;
static bool item_count(const void* data,size_t* count)
{const Item* item=data;if(!item||!item->references||!count)return false;*count=item->references;return true;}
static bool item_trace(const void* data,xrtownershipvisitor visit,ptr context)
{(void)data;(void)context;return visit!=NULL;}
static void item_run(ptr data)
{Item* item=data;assert(item->references==1);(void)xrtAtomic32FetchAdd(item->completed,1,XMEMORY_RELEASE);}
static void item_drop(ptr data,ptr context)
{
    Item* item=data;assert(!context&&item->references==1);xatomic32* completed=item->completed;
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    item->references=0;xrtFree(item);assert(xrtOwnershipScopeEnd(&mutation));
    (void)xrtAtomic32FetchAdd(completed,1,XMEMORY_RELEASE);
}
static const xrtownershipops item_ops={item_count,item_trace};
static const xexecutordataownershipv1 item_policy={sizeof(item_policy),item_run,item_drop,&item_ops};
static bool remember(xrtownershipref ref,ptr data)
{Slots* slots=data;assert(slots->count<2);slots->items[slots->count++]=ref;return true;}
static void freeze_begin(xrtownershipscope* scope)
{
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
}
int main(void)
{
    assert(xrtMemDebugEnable(true));assert(xrtTemp(8));assert(xrtTempClear());xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);xatomic32 completed;xrtAtomic32Init(&completed,0);
    xexecutorconfig config={1,2,0};xexecutor* pool=xrtExecutorCreate(&config);assert(pool);
    xrtownershipref ref=xrtExecutorOwnership(pool);xrtownershipscope freeze={0};freeze_begin(&freeze);size_t count;
    const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipadapterv1* adapter=xrtExecutorOwnershipAdapterV1(ref,&preparation);
    assert(adapter&&preparation&&adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&config));
    assert(xrtExecutorOwnershipQuiescenceV1Get()->Ops==ref.Ops&&!xrtExecutorOwnershipQuiescenceV1Get()->Busy(ref.Data));
    Item* first=xrtCalloc(1,sizeof(*first));Item* second=xrtCalloc(1,sizeof(*second));assert(first&&second);
    *first=(Item){1,&completed};*second=(Item){1,&completed};
    assert(xrtExecutorSubmitOwnedV1(pool,first,&item_policy));xexecutorowneditemv1 batch={second,&item_policy};
    assert(xrtExecutorSubmitOwnedBatchV1(pool,&batch,1));
    assert(ref.Ops->Count(ref.Data,&count)&&count==4);Slots slots={0};assert(ref.Ops->Trace(ref.Data,remember,&slots)&&slots.count==2);
    const xexecutordataownershipv1* policies[]={&item_policy};
    const xrtownershipadapterv1* job_adapters[2];const xrtownershippreparationv1* job_preparations[2];
    for(size_t i=0;i<2;++i){xrtownershipref job=slots.items[i];
        assert(xrtExecutorJobOwnershipQuiescenceV1Get()->Ops==job.Ops&&!xrtExecutorJobOwnershipQuiescenceV1Get()->Busy(job.Data));
        const xrtownershippreparationv1* sentinel=(const xrtownershippreparationv1*)&slots;
        assert(!xrtExecutorJobOwnershipAdapterV1(job,NULL,0,&sentinel)&&sentinel==(const xrtownershippreparationv1*)&slots);
        job_adapters[i]=xrtExecutorJobOwnershipAdapterV1(job,policies,1,&job_preparations[i]);
        assert(job_adapters[i]&&job_adapters[i]->Hold(job.Data)&&job_adapters[i]->Claim(job.Data,&config));
        assert(job.Ops->Count(job.Data,&count)&&count==2&&job_preparations[i]->Prepare(job.Data,&config)==XRT_OWNERSHIP_PREPARE_BUSY);
    }
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(&ref,1,NULL,0,&graph)&&graph.NodeCount==5&&graph.EdgeCount==6);
    assert(preparation->Prepare(ref.Data,&config)==XRT_OWNERSHIP_PREPARE_BUSY);assert(xrtOwnershipScopeEnd(&freeze));
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){xrtownershipprepareresult result=preparation->Prepare(ref.Data,&config);
        if(result==XRT_OWNERSHIP_PREPARE_READY)break;
        assert(result==XRT_OWNERSHIP_PREPARE_BUSY&&!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    freeze_begin(&freeze);assert(preparation->Ready(ref.Data)&&xrtAtomic32Load(&completed,XMEMORY_ACQUIRE)==4);
    for(size_t i=0;i<2;++i){assert(job_preparations[i]->Ready(slots.items[i].Data));job_adapters[i]->Clear(slots.items[i].Data,&config);}
    adapter->Clear(ref.Data,&config);assert(!adapter->Hold(ref.Data));assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(ref.Data,&config)&&xrtExecutorDestroy(pool));
    for(size_t i=0;i<2;++i){assert(job_adapters[i]->Finish(slots.items[i].Data,&config));job_adapters[i]->Drop(slots.items[i].Data);}
    adapter->Drop(ref.Data);xrtClearError();xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before.LiveCount&&after.LiveBytes==before.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        after.InvalidFreeCount==before.InvalidFreeCount&&after.DoubleFreeCount==before.DoubleFreeCount&&
        after.UseAfterFreeCount==before.UseAfterFreeCount);
    puts("Executor native prerequisite: seven APIs, five physical nodes, six edges, exact cleanup and allocation balance");
    return 0;
}
