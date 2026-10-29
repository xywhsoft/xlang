#define XRT_MODULE_MEMORY
#define XRT_MODULE_ERROR
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_continuation_future.h"

static xpromise* gate;
static xfuture* gateSource;

static bool freeze_begin(xrtownershipscope* freeze,xdeadline deadline)
{
    while(!xrtOwnershipFreezeTryBegin(freeze)){
        if(xrtDeadlineExpired(deadline)){xrtSetErrorKind(XERR_STATE);return false;}
        xrtThreadYield();
    }
    return true;
}
static bool parked_frame(xrtownershipref ref,void* context)
{
    const xrtownershippreparationv1* preparation=NULL;
    if(xl6ContinuationFrameOwnershipAdapterV1(ref,&preparation))*(bool*)context=true;
    return true;
}

xfuture* xlir_void_gate_open(void)
{
    xfuture* future = NULL;xrtownershipscope freeze={0};
    if(!freeze_begin(&freeze,xrtDeadlineAfter(5000000)))return NULL;
    if (gate != NULL) {
        xrtOwnershipScopeEnd(&freeze);xrtSetErrorKind(XERR_STATE);return NULL;
    }
    gate = xrtPromiseCreate(&future, NULL);
    gateSource=future?xrtFutureRef(future):NULL;
    xrtOwnershipScopeEnd(&freeze);
    return future;
}

bool xlir_void_gate_finish(int64_t outcome)
{
    bool ok = false;
    xerror* error;xpromise* promise=NULL;xfuture* source=NULL;
    xdeadline deadline=xrtDeadlineAfter(5000000);
    /* Async roots start on workers. Complete only after the actual pending
     * source has a parked, inspectable continuation Watch. The shared freeze
     * also serializes publication/access of these test-owned native handles. */
    for(;;){
        xrtownershipscope freeze={0};bool parked=false;
        if(!freeze_begin(&freeze,deadline))return false;
        if(gateSource){
            xrtownershipref ref=xrtFutureOwnership(gateSource);
            if(ref.Ops->Trace(ref.Data,parked_frame,&parked)&&parked){
                promise=gate;source=gateSource;gate=NULL;gateSource=NULL;
            }
        }
        xrtOwnershipScopeEnd(&freeze);
        if(promise)break;
        if(xrtDeadlineExpired(deadline)){xrtSetErrorKind(XERR_STATE);return false;}
        xrtThreadYield();
    }
    switch (outcome) {
    case 0: ok = xrtPromiseResolve(promise, NULL); break;
    case 1:
        error = xrtErrorCreate(XERR_STATE, "void gate", 1, "gate failed");
        if (error != NULL) { ok = xrtPromiseReject(promise, error); xrtErrorFree(error); }
        break;
    case 2: ok = xrtPromiseCancel(promise); break;
    case 3: ok = xrtPromiseClose(promise); break;
    default: xrtSetErrorKind(XERR_ARGUMENT); break;
    }
    xrtPromiseDestroy(promise);xrtFutureDestroy(source);
    return ok;
}
