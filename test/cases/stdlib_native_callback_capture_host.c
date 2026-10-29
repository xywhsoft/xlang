#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_native_future_payload.h"
#include "src/runtime/xl6_slot_map_key_capture.h"
#include "src/runtime/xl6_slot_map_key.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct Payload { XL6_SlotMapKeyV1 key;unsigned mode; } Payload;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_NATIVE_LEAF_V2,offsetof(Payload,key),sizeof(XL6_SlotMapKeyV1)},xl6SlotMapKeyCaptureProfileV1Get},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Payload,mode),sizeof(unsigned)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Payload),2,slots};
static const XL6_TaskRuntimeV3* task;
static const XL6_FutureCallbackRuntimeV4* callback;
static const XL6_NativeFuturePayloadRuntimeV1* native;
static unsigned vectors,runs,refusals,edges;
static bool visit(xrtownershipref ref,void* unused)
{ (void)unused;if(ref.Data){assert(ref.Ops);++edges;}return true; }
static void initialize(Payload* p,unsigned mode)
{
    *p=(Payload){0};p->mode=mode;p->key.instance=xl6SlotMapInstanceCreateV1();assert(p->key.instance);
    p->key.slot=UINT64_C(0x100000001);
}
static XL6_SlotMapKeyV1* prepare_result(Payload* p)
{
    XL6_SlotMapKeyV1* result=native->Allocate(xl6SlotMapKeyCaptureProfileV1Get());assert(result);
    assert(xl6SlotMapKeyCopyValueV1(p->key,result));return result;
}
static void invoke(const xfutureresult* input,xpromise* output,xcancel* cancel,void* data)
{
    (void)cancel;Payload* p=data;assert(input);++runs;
    XL6_SlotMapKeyV1* result=input->State==XFUTURE_RESOLVED ? prepare_result(p) : NULL;
    assert(xrtMemDebugFailAfter(0));callback->Cleanup(p);callback->Cleanup(p);
    assert(!p->key.instance && !p->key.slot && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    if(result)assert(native->ResolveOwned(output,result));else assert(xrtPromiseCancel(output));
}
static XL6_TaskOutcome run_task(xcancel* cancel,void* data,XL6_TaskResult* result)
{
    (void)cancel;Payload* p=data;XL6_SlotMapKeyV1* value=prepare_result(p);
    assert(xrtMemDebugFailAfter(0));task->Cleanup(p);task->Cleanup(p);
    assert(!p->key.instance && !p->key.slot && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    result->Value=value;result->Destroy=native->Drop;result->DestroyData=NULL;return XL6_TASK_SUCCESS;
}
static void scenario(unsigned mode)
{
    if(mode<3) {
        Payload* p=callback->Allocate(NULL,sizeof(*p),invoke,&layout);assert(p);initialize(p,mode);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        if(mode==1)assert(xrtPromiseResolve(promise,NULL));
        xfuture* output=xl6FutureCallbackRuntimeV1Get()->Start(p,source);assert(output);
        const xrtownershipadapterv1* pin=NULL;xrtownershipref ref={0};
        if(mode!=1) {
            ref=xl6FutureCallbackRuntimeV1Get()->Ownership(p);const xrtownershippreparationv1* preparation=NULL;
            pin=xl6FutureCallbackOwnershipAdapterV1(ref,&preparation);assert(pin && preparation && pin->Hold(ref.Data));
            edges=0;assert(ref.Ops->Trace(ref.Data,visit,NULL) && edges==3);
            if(mode==2)assert(xrtPromiseCancel(promise));else assert(xrtPromiseResolve(promise,NULL));
            assert(!p->key.instance && !p->key.slot);size_t count=0;assert(ref.Ops->Count(ref.Data,&count) && count==1);
            edges=0;assert(ref.Ops->Trace(ref.Data,visit,NULL) && !edges);pin->Drop(ref.Data);
        }
        assert(xrtFutureState(output)==(mode==2 ? XFUTURE_CANCELLED : XFUTURE_RESOLVED));
        if(mode!=2) {XL6_SlotMapKeyV1 owned={0};assert(native->Copy(xrtFutureValue(output),xl6SlotMapKeyCaptureProfileV1Get(),&owned));assert(owned.instance);xl6SlotMapKeyDropValueV1(owned);}
        xrtPromiseDestroy(promise);xrtFutureDestroy(source);xrtFutureDestroy(output);
    } else if(mode==3) {
        Payload* p=task->Allocate(NULL,sizeof(*p),run_task,&layout);assert(p);initialize(p,mode);
        XL6_TaskPoolConfig config={1,1,0};XL6_TaskPool* pool=xl6TaskRuntimeV1Get()->PoolCreate(&config);assert(pool);
        xfuture* output=xl6TaskSubmitOwnedPolicyV1(pool,p,NULL,native->Policy(),true);assert(output);
        assert(xrtFutureWait(output)==XWAIT_OK);assert(xl6TaskRuntimeV1Get()->PoolQuiesce(pool));
        XL6_SlotMapKeyV1 owned={0};assert(native->Copy(xrtFutureValue(output),xl6SlotMapKeyCaptureProfileV1Get(),&owned));
        xrtFutureDestroy(output);assert(owned.instance);xl6SlotMapKeyDropValueV1(owned);assert(xl6TaskRuntimeV1Get()->PoolDestroy(pool));
    } else {
        Payload* p=callback->Allocate(NULL,sizeof(*p),invoke,&layout);assert(p);initialize(p,mode);
        assert(xrtMemDebugFailAfter(0));xl6FutureCallbackRuntimeV1Get()->Discard(p);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
        p=task->Allocate(NULL,sizeof(*p),run_task,&layout);assert(p);initialize(p,mode);
        assert(xrtMemDebugFailAfter(0));xl6TaskRuntimeV1Get()->Discard(p);assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    }
    assert(!xrtGetError());++vectors;
}
int main(int argc,char** argv)
{
    task=xl6TaskRuntimeV3Get();callback=xl6FutureCallbackRuntimeV4Get();native=xl6NativeFuturePayloadRuntimeV1Get();xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==2) {
        size_t point=(size_t)strtoull(argv[1],NULL,10);assert(xrtMemDebugFailAfter(point));
        Payload* p=callback->Allocate(NULL,sizeof(*p),invoke,&layout);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        assert((p!=NULL)==!hit);if(p){*p=(Payload){0};xl6FutureCallbackRuntimeV1Get()->Discard(p);}xrtClearError();
        printf("prefix=%zu hit=%u\n",point,(unsigned)hit);
    } else {
        assert(argc==1);XL6_CaptureLayoutV2 bad=layout;bad.bytes--;
        assert(!callback->Allocate(NULL,sizeof(Payload),invoke,&bad));++refusals;xrtClearError();
        assert(!task->Allocate(NULL,sizeof(Payload),run_task,&bad));++refusals;xrtClearError();
        for(unsigned i=0;i<200;++i)for(unsigned mode=0;mode<5;++mode)scenario(mode);
        printf("vectors=%u refusals=%u callbacks=%u\n",vectors,refusals,runs);
    }
    assert(!xl6ProductMemoryEnd());xl6ProductMemoryVerify();return 0;
}
