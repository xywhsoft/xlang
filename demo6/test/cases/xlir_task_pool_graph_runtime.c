#define XRT_MODULE_TASK_POOL
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_task.h"
#include <assert.h>
#include <stdio.h>

typedef struct Payload { unsigned* runs;xfuture* held; } Payload;
static const XL6_CaptureSlot slots[]={
    {XL6_CAPTURE_TRIVIAL,offsetof(Payload,runs),sizeof(unsigned*)},
    {XL6_CAPTURE_FUTURE,offsetof(Payload,held),sizeof(xfuture*)}};
static const XL6_CaptureLayoutV1 layout={sizeof(layout),sizeof(Payload),2,slots};
static const XL6_TaskRuntimeV1* api;
static unsigned transactions,queuedGraphs,preparations;
static XL6_TaskOutcome run(xcancel* cancel,void* data,XL6_TaskResult* result)
{
    Payload* payload=data;(void)cancel;++*payload->runs;
    int64_t* number=xrtMalloc(sizeof(*number));assert(number);*number=19;
    *result=(XL6_TaskResult){number,xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_VALUE),NULL};return XL6_TASK_SUCCESS;
}
static Payload* allocate(unsigned* runs)
{
    Payload* payload=xl6TaskRuntimeV2Get()->Allocate(NULL,sizeof(*payload),run,&layout);
    assert(payload);*payload=(Payload){runs,NULL};return payload;
}
static bool native_slot(xrtownershipref ref,void* data)
{
    xrtownershipref* native=data;assert(!native->Data&&ref.Data);*native=ref;return true;
}
static void quiet(XL6_TaskPool* pool,xrtownershipscope* freeze,xrtownershipref* native)
{
    xdeadline end=xrtDeadlineAfter(5000000);xrtownershipref root=xl6TaskPoolOwnership(pool);
    for(;;){
        if(xrtOwnershipFreezeTryBegin(freeze)){
            size_t count;*native=(xrtownershipref){0};
            if(root.Ops->Count(root.Data,&count)&&root.Ops->Trace(root.Data,native_slot,native)&&
                (!native->Data||native->Ops->Count(native->Data,&count)))return;
            assert(xrtOwnershipScopeEnd(freeze));
        }
        assert(!xrtDeadlineExpired(end));xrtThreadYield();
    }
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot now;xrtClearError();xrtMemDebugSnapshot(&now);
    assert(before->LiveCount==now.LiveCount&&before->LiveBytes==now.LiveBytes&&
        now.AllocCount-before->AllocCount==now.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==now.InvalidFreeCount&&before->DoubleFreeCount==now.DoubleFreeCount&&
        before->UseAfterFreeCount==now.UseAfterFreeCount);++transactions;
}
static void transaction(unsigned jobs,bool language,bool nativeFirst)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned runs=0,token;
    XL6_TaskPoolConfig config={1,8,0};XL6_TaskPool* pool=api->PoolCreate(&config);assert(pool);
    if(jobs){
        xfuture* warm=xl6TaskSubmitPayloadV1(pool,allocate(&runs),NULL,XL6_FUTURE_PAYLOAD_VALUE,false);
        assert(warm&&xrtFutureWaitFor(warm,5000000)==XWAIT_OK);xrtFutureDestroy(warm);
    }
    xrtownershipscope freeze={0};xrtownershipref native={0};quiet(pool,&freeze,&native);
    xrtownershipref root=xl6TaskPoolOwnership(pool);size_t count=0;
    assert(root.Ops->Count(root.Data,&count)&&count==1);
    for(unsigned i=0;i<jobs;++i){
        Payload* payload=allocate(&runs);
        xfuture* output=xl6TaskSubmitPayloadV1(pool,payload,NULL,XL6_FUTURE_PAYLOAD_VALUE,language);assert(output);
        payload->held=xrtFutureRef(output);assert(payload->held);xrtFutureDestroy(output);
    }
    assert(root.Ops->Count(root.Data,&count)&&count==1+jobs);
    const xrtownershippreparationv1 *poolPrep=NULL,*nativePrep=NULL;
    const xrtownershipadapterv1* poolAdapter=xl6TaskPoolOwnershipAdapterV1(root,&poolPrep);
    const xrtownershipadapterv1* nativeAdapter=native.Data?xrtTaskPoolOwnershipAdapterV1(native,&nativePrep):NULL;
    assert(poolAdapter&&poolPrep&&poolAdapter->Hold(pool));
    if(native.Data)assert(nativeAdapter&&nativePrep&&nativeAdapter->Hold(native.Data));
    xrtownershipref internal[3];internal[0]=root;internal[1]=root;internal[2]=native;
    xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
    if(jobs){
        printf("Pool graph jobs=%u language=%u nodes=%llu edges=%llu\n",jobs,(unsigned)language,
            (unsigned long long)graph.NodeCount,(unsigned long long)graph.EdgeCount);
        assert(graph.NodeCount==2+jobs*(language?6u:4u)&&graph.EdgeCount==1+jobs*(language?14u:8u));++queuedGraphs;
    }else assert(graph.NodeCount==1&&graph.EdgeCount==0);
    /* Internal anchors are counted as edges by Inspect. Check the physical
     * graph above, then independently account for the owner and real pins. */
    assert(xrtOwnershipInspect(&root,1,internal,native.Data?3:2,&graph)&&graph.ExternalRootCount==0);
    assert(poolAdapter->Claim(pool,&token));if(native.Data)assert(nativeAdapter->Claim(native.Data,&token));
    assert(xrtOwnershipScopeEnd(&freeze));
    xdeadline settled=xrtDeadlineAfter(5000000);
    while(poolPrep->Prepare(pool,&token)!=XRT_OWNERSHIP_PREPARE_READY){assert(!xrtDeadlineExpired(settled));xrtThreadYield();}
    ++preparations;
    if(native.Data){
        xdeadline end=xrtDeadlineAfter(5000000);
        while(nativePrep->Prepare(native.Data,&token)!=XRT_OWNERSHIP_PREPARE_READY){assert(!xrtDeadlineExpired(end));xrtThreadYield();}
        ++preparations;
    }
    quiet(pool,&freeze,&native);
    assert(runs==(jobs?jobs+1:0)&&poolPrep->Ready(pool)&&(!native.Data||nativePrep->Ready(native.Data)));
    assert(root.Ops->Count(root.Data,&count)&&count==2);
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&
        graph.NodeCount==(native.Data?2u:1u)&&graph.EdgeCount==(native.Data?1u:0u));
    assert(xrtOwnershipInspect(&root,1,internal,native.Data?3:2,&graph)&&graph.ExternalRootCount==0);
    poolAdapter->Clear(pool,&token);if(native.Data)nativeAdapter->Clear(native.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze));
    if(native.Data&&nativeFirst)assert(nativeAdapter->Finish(native.Data,&token));
    assert(poolAdapter->Finish(pool,&token)&&poolAdapter->Finish(pool,&token));
    if(native.Data&&!nativeFirst)assert(nativeAdapter->Finish(native.Data,&token));
    assert(api->PoolIsClosing(pool)&&api->PoolIsQuiescent(pool)&&api->PoolDestroy(pool));
    if(native.Data)nativeAdapter->Drop(native.Data);
    poolAdapter->Drop(pool);balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));api=xl6TaskRuntimeV1Get();
    xrtSetErrorKind(XERR_STATE);xrtClearError();
    for(unsigned n=0;n<40;++n){transaction(0,false,false);for(unsigned jobs=1;jobs<=3;++jobs)
        for(unsigned language=0;language<2;++language)transaction(jobs,language!=0,(n&1)!=0);}
    assert(transactions==280&&queuedGraphs==240&&preparations==520);
    puts("Task pool wrapper: 280 balanced lifetimes, 240 exact queued raw/language graphs, 520 real preparations, accepted execution, joined native owner and both Finish orders");
    return 0;
}
