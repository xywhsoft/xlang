#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_future_payload.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

typedef struct Hooks { volatile LONG calls, cancelled; xpromise* promise; bool completeOnCancel; } Hooks;
typedef struct Capture { Hooks* hooks; xfuture* source; } Capture;
static const XL6_CaptureSlot slots[] = {
    {XL6_CAPTURE_TRIVIAL, offsetof(Capture,hooks), sizeof(Hooks*)},
    {XL6_CAPTURE_FUTURE, offsetof(Capture,source), sizeof(xfuture*)}
};
static const XL6_CaptureLayoutV1 layout = {sizeof(layout),sizeof(Capture),2,slots};
static const XL6_FutureCallbackRuntimeV1* api;
static const XL6_FutureCallbackRuntimeV2* storage;
static const XL6_FutureCallbackRuntimeV3* exclusive;

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && after.OverflowCount==before->OverflowCount &&
        after.UnderflowCount==before->UnderflowCount);
}
static void complete(xpromise* promise,unsigned outcome)
{
    if(outcome==0)assert(xrtPromiseResolve(promise,NULL));
    else if(outcome==2)assert(xrtPromiseCancel(promise));
    else if(outcome==3)assert(xrtPromiseClose(promise));
    else{xrtSetErrorKind(XERR_STATE);assert(xrtPromiseReject(promise,xrtGetError()));xrtClearError();}
}
static void callback(const xfutureresult* input,xpromise* output,xcancel* cancel,void* data)
{
    Capture* frame=data;Hooks* hooks=frame->hooks;assert(input);
    InterlockedIncrement(&hooks->calls);
    storage->Cleanup(frame);
    if(xrtCancelRequested(cancel) || input->State==XFUTURE_CANCELLED){(void)xrtPromiseCancel(output);return;}
    if(input->State==XFUTURE_FAILED){assert(input->Error);(void)xrtPromiseReject(output,input->Error);return;}
    if(input->State!=XFUTURE_RESOLVED){(void)xrtPromiseClose(output);return;}
    int* box=xrtMalloc(sizeof(*box));assert(box);*box=207;
    if(!xl6PayloadResolveOwned(output,box,XL6_FUTURE_PAYLOAD_VALUE))xrtFree(box);
}
static Capture* allocate(xfuture* source,Hooks* hooks)
{
    Capture* frame=storage->Allocate(NULL,sizeof(*frame),callback,&layout);assert(frame);
    frame->hooks=hooks;frame->source=xrtFutureRef(source);assert(frame->source);return frame;
}
static void inspect(Capture* frame)
{
    xrtownershipscope freeze={0};xrtownershipresult graph={0};size_t refs=0;
    xrtownershipref root=api->Ownership(frame);
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(root.Ops->Count(root.Data,&refs) && refs==3);
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
    assert(graph.NodeCount==6 && graph.EdgeCount==11 && graph.ExternalRootCount==2);
    const xrtownershippreparationv1* preparation=NULL;
    assert(xl6FutureCallbackOwnershipAdapterV1(root,&preparation) && preparation);
    assert(xrtOwnershipScopeEnd(&freeze));
}
static void pending_and_ready(void)
{
    for(unsigned n=0;n<100;++n)for(unsigned ready=0;ready<2;++ready){
        xmemdebugsnapshot before;Hooks hooks={0};xfuture *source=NULL,*output;
        xrtMemDebugSnapshot(&before);xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        Capture* frame=allocate(source,&hooks);if(ready)complete(promise,n%4);
        output=exclusive->StartExclusive(frame,source);assert(output);
        if(!ready){inspect(frame);complete(promise,n%4);}
        assert(hooks.calls==1 && xrtFutureState(output)==(n%4==0?XFUTURE_RESOLVED:n%4==1?XFUTURE_FAILED:n%4==2?XFUTURE_CANCELLED:XFUTURE_CLOSED));
        xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    }
}
static void requested(void* data)
{
    Hooks* hooks=data;InterlockedIncrement(&hooks->cancelled);
    if(hooks->completeOnCancel)assert(xrtPromiseCancel(hooks->promise));
}
static void cancellation(void)
{
    for(unsigned n=0;n<100;++n)for(unsigned immediate=0;immediate<2;++immediate){
        xmemdebugsnapshot before;Hooks hooks={0};xfuture *source=NULL,*output;
        xrtMemDebugSnapshot(&before);hooks.promise=xrtPromiseCreate(&source,NULL);assert(hooks.promise);
        xcancel* cancel=xrtPromiseCancelToken(hooks.promise);assert(cancel);
        hooks.completeOnCancel=!!immediate;xcancelwatch* observer=xrtCancelWatch(cancel,requested,&hooks);assert(observer);
        output=exclusive->StartExclusive(allocate(source,&hooks),source);assert(output);
        xrtSetErrorKind(XERR_STATE);const xerror* diagnostic=xrtGetError();assert(diagnostic);
        assert(xrtFutureCancel(output) && xrtGetError()==diagnostic);xrtClearError();
        assert(hooks.cancelled==1 && xrtCancelRequested(cancel));
        if(!immediate){assert(!hooks.calls && xrtFutureState(output)==XFUTURE_PENDING);complete(hooks.promise,0);}
        assert(hooks.calls==1 && xrtFutureState(output)==XFUTURE_CANCELLED);
        xrtCancelUnwatch(observer);xrtCancelDestroy(cancel);xrtFutureDestroy(output);
        xrtPromiseDestroy(hooks.promise);xrtFutureDestroy(source);balanced(&before);
    }
    /* The original non-exclusive entry MUST NOT request source cancellation. */
    for(unsigned n=0;n<100;++n){
        xmemdebugsnapshot before;Hooks hooks={0};xfuture *source=NULL,*output;
        xrtMemDebugSnapshot(&before);xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        xcancel* cancel=xrtPromiseCancelToken(promise);assert(cancel);
        output=api->Start(allocate(source,&hooks),source);assert(output && xrtFutureCancel(output));
        assert(!xrtCancelRequested(cancel) && !hooks.calls);complete(promise,0);
        assert(hooks.calls==1 && xrtFutureState(output)==XFUTURE_CANCELLED);
        xrtCancelDestroy(cancel);xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    }
}
static unsigned allocation_faults(void)
{
    unsigned faults=0;
    for(unsigned ready=0;ready<2;++ready)for(unsigned budget=0;budget<128;++budget){
        xmemdebugsnapshot before;Hooks hooks={0};xfuture *source=NULL,*output;
        xrtMemDebugSnapshot(&before);xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        Capture* frame=allocate(source,&hooks);if(ready)complete(promise,2);
        assert(xrtMemDebugFailAfter(budget));output=exclusive->StartExclusive(frame,source);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(!output){assert(triggered && !hooks.calls && xrtGetError());++faults;api->Discard(frame);xrtClearError();}
        else{assert(!triggered);if(!ready)complete(promise,2);assert(hooks.calls==1);xrtFutureDestroy(output);}
        xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
        if(!triggered)break;
        assert(budget!=127);
    }
    assert(faults>0);return faults;
}
typedef struct Race { xfuture* output;xpromise* promise;volatile LONG go;bool cancel; } Race;
static DWORD WINAPI racing(void* data)
{
    Race* race=data;
    while(!InterlockedCompareExchange(&race->go,0,0))SwitchToThread();
    if(race->cancel)(void)xrtFutureCancel(race->output);else complete(race->promise,0);
    /* This foreign thread owns only its current diagnostic. Runtime storage
     * retirement is terminal for the entire instance, not thread cleanup. */
    xrtClearError();return 0;
}
static void races(void)
{
    for(unsigned n=0;n<1000;++n){
        xmemdebugsnapshot before;Hooks hooks={0};xfuture *source=NULL,*output;Race first={0},second={0};
        xrtMemDebugSnapshot(&before);xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        output=exclusive->StartExclusive(allocate(source,&hooks),source);assert(output);
        first.output=output;first.cancel=true;second.promise=promise;
        HANDLE threads[2]={CreateThread(NULL,0,racing,&first,0,NULL),CreateThread(NULL,0,racing,&second,0,NULL)};
        assert(threads[0] && threads[1]);InterlockedExchange(&first.go,1);InterlockedExchange(&second.go,1);
        assert(WaitForMultipleObjects(2,threads,TRUE,10000)==WAIT_OBJECT_0);
        assert(CloseHandle(threads[0]) && CloseHandle(threads[1]));
        assert(hooks.calls==1 && (xrtFutureState(output)==XFUTURE_RESOLVED || xrtFutureState(output)==XFUTURE_CANCELLED));
        xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    }
}
int main(void)
{
    xmemdebugsnapshot before;assert(xrtMemDebugEnable(true));
    api=xl6FutureCallbackRuntimeV1Get();storage=xl6FutureCallbackRuntimeV2Get();exclusive=xl6FutureCallbackRuntimeV3Get();
    xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    pending_and_ready();cancellation();unsigned faults=allocation_faults();races();
    xrtSetErrorKind(XERR_STATE);assert(xrtGetError());xrtClearError();balanced(&before);
    printf("Exclusive Future callbacks: 200 outcomes, 100 exact pending graphs, 200 source cancellations, 100 non-exclusive controls, %u allocation rollbacks, 1000 cancellation/completion races; zero live delta\n",faults);
    /* All racing threads have physically exited and no XRT entry follows. */
    assert(xrtRuntimeRetireThreadStorage());return 0;
}
