#include "xl6_net_resolver_input.h"
#include "xl6_native_handle.h"
#include "xl6_future_callback.h"
#include "xl6_future_payload.h"
void xl6NetResolveOperationDestroyV1(void* raw){xrtFutureDestroy(raw);}
bool xl6NetResolveOperationDoneV1(const void* raw){return xrtFutureState(raw)!=XFUTURE_PENDING;}
bool xl6NetResolveOperationCancelV1(void* raw){return xrtFutureCancel(raw);}
static void completed(const xfutureresult* input,xpromise* output,xcancel* cancel,void* data)
{
    (void)data;
    if(xrtCancelRequested(cancel)){(void)xrtPromiseCancel(output);return;}
    if(!input){xl6FutureCallbackFailV1(output,xrtTakeError());return;}
    if(input->State==XFUTURE_CANCELLED){(void)xrtPromiseCancel(output);return;}
    if(input->State==XFUTURE_CLOSED){(void)xrtPromiseClose(output);return;}
    if(input->State==XFUTURE_FAILED){xl6FutureCallbackFailV1(output,xrtErrorRef(input->Error));return;}
    if(input->State!=XFUTURE_RESOLVED){xrtSetErrorKind(XERR_STATE);xl6FutureCallbackFailV1(output,xrtTakeError());return;}
    bool* value=xrtMalloc(sizeof(*value));
    if(!value){xl6FutureCallbackFailV1(output,xrtTakeError());return;}
    *value=true;
    if(!xl6PayloadResolveOwned(output,value,XL6_FUTURE_PAYLOAD_VALUE)){
        xerror* error=xrtTakeError();xrtFree(value);xl6FutureCallbackFailV1(output,error);
    }
}
xfuture* xl6NetResolveOperationWaitV1(void* raw)
{
    /* There are no captures, module callbacks, native view or class-layout
     * descriptors in this resident bridge. Start owns the actual source and
     * the shared certified callback node/Watch; cancellation requests the
     * same operation, never manufactures native worker-exit readiness. */
    /* Capture storage is nonempty by the shared ABI; the one resident trivial
     * marker owns nothing. No zero-byte special case is added to the core. */
    static const XL6_CaptureSlotV2 marker[]={{{XL6_CAPTURE_TRIVIAL,0,sizeof(unsigned char)},NULL}};
    static const XL6_CaptureLayoutV2 empty={sizeof(empty),sizeof(unsigned char),1,marker};
    void* frame=xl6FutureCallbackRuntimeV4Get()->Allocate(NULL,sizeof(unsigned char),completed,&empty);
    if(!frame){xl6NetResolverFailedV1();return NULL;}
    *(unsigned char*)frame=0;
    xfuture* result=xl6FutureCallbackRuntimeV3Get()->StartExclusive(frame,raw);
    if(!result){
        xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
        xl6FutureCallbackRuntimeV1Get()->Discard(frame);
        xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);xl6ErrorScopePropagateFailure(outcome);
    }
    return result;
}
void* xl6NetResolveOperationResultV1(const void* raw,int64_t port)
{
    uint16 number;if(!xl6NetResolverPortV1(port,&number)){xl6NetResolverFailedV1();return NULL;}
    xfutureresult value={0};
    if(!xrtFutureResult(raw,&value)||!xl6PayloadObserveOutcome(value.State,value.Error)){
        xl6NetResolverFailedV1();return NULL;}
    if(!value.Value){xrtSetErrorKind(XERR_STATE);xl6NetResolverFailedV1();return NULL;}
    xnetaddrlist* result=number?xrtNetAddrListWithPort(value.Value,number):xrtNetAddrListRef(value.Value);
    if(!result)xl6NetResolverFailedV1();
    return result;
}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&
        ops==xl6NetResolveOperationHandlePolicyV1Get()->Ops&&visit(xrtFutureOwnership(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetResolveOperationHandlePolicyV1Get,xl6NetResolveOperationDestroyV1,trace)
