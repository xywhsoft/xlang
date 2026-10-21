#include "xl6_tcp_receive_into_internal.h"
#include "xl6_native_class_borrow.h"
#include "xl6_future_callback.h"
#include "xl6_future_payload.h"
typedef struct ReceiveIntoCapture {
    xvalue *stream,*target;
    const XL6_NativeHandlePolicyV1* policy;
} ReceiveIntoCapture;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(ReceiveIntoCapture,stream),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_XVALUE,offsetof(ReceiveIntoCapture,target),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(ReceiveIntoCapture,policy),sizeof(void*)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(ReceiveIntoCapture),3,slots};
static bool target(const ReceiveIntoCapture* capture,void** output)
{
    void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!xrtValueGetHandle(capture->target,output,&ops,&user)||!*output||user||ops!=capture->policy->Ops){
        xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
static void completed(const xfutureresult* input,xpromise* output,xcancel* cancel,void* raw)
{
    ReceiveIntoCapture* capture=raw;int64_t* box=NULL;
    xl6_error_result outcome={true,NULL};xfuturestate terminal=XFUTURE_RESOLVED;
    if(!input){xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};}
    else if(xrtCancelRequested(cancel)||input->State==XFUTURE_CANCELLED)terminal=XFUTURE_CANCELLED;
    else if(input->State==XFUTURE_FAILED)outcome=(xl6_error_result){false,xrtErrorRef(input->Error)};
    else if(input->State!=XFUTURE_RESOLVED&&input->State!=XFUTURE_CLOSED){
        xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};
    }else{
        box=xrtCalloc(1,sizeof(*box));bool ok=box!=NULL;void* buffer=NULL;
        if(ok)ok=target(capture,&buffer);
        if(ok&&input->State==XFUTURE_RESOLVED){
            if(!input->Value){xrtSetErrorKind(XERR_STATE);ok=false;}
            else{xbytesview view=xrtNetBytesView(input->Value);
                ok=xl6TcpReceiveIntoAppendInternalV1(buffer,view);if(ok)*box=(int64_t)view.Size;}
        }
        if(!ok)outcome=(xl6_error_result){false,xrtTakeError()};
        /* Keep no language callbacks or field-name metadata after submission. */
    }
    outcome=xl6FutureCallbackRuntimeV5Get()->Cleanup(raw,outcome);
    if(!outcome.ok){xrtFree(box);xl6FutureCallbackFailV1(output,outcome.error);return;}
    if(terminal==XFUTURE_CANCELLED){(void)xrtPromiseCancel(output);return;}
    if(!xl6PayloadResolveOwned(output,box,XL6_FUTURE_PAYLOAD_VALUE)){
        xrtFree(box);xl6FutureCallbackFailV1(output,xrtTakeError());}
}
static xfuture* failed(ReceiveIntoCapture* capture,xfuture* source)
{
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    if(source&&xrtFutureState(source)==XFUTURE_PENDING)(void)xrtFutureCancel(source);
    xl6FutureCallbackRuntimeV1Get()->Discard(capture);xrtFutureDestroy(source);
    xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&cleanup));return NULL;
}
xfuture* xl6TcpReceiveIntoStartInternalV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,int64_t maximum,const XL6_NativeHandlePolicyV1* policy)
{
    size_t count;if(!xl6TcpPositiveSizeV1(maximum,&count))return failed(NULL,NULL);
    ReceiveIntoCapture* capture=xl6FutureCallbackRuntimeV5Get()->Allocate(NULL,sizeof(*capture),completed,&layout);
    if(!capture)return failed(NULL,NULL);
    *capture=(ReceiveIntoCapture){.policy=policy};void* buffer=NULL;
    capture->target=xl6NativeClassFieldRetainV1(context,object,policy,&buffer);
    if(!capture->target||!xl6TcpReceiveIntoPrepareInternalV1(buffer,&count))return failed(capture,NULL);
    void* owned=xl6ExecutionResourceRetainV1(receiver);
    if(!owned)return failed(capture,NULL);
    capture->stream=xrtValueHandleTake(&owned,xl6TcpStreamHandlePolicyV1Get()->Ops,NULL);
    if(!capture->stream){xl6ExecutionResourceReleaseV1(owned);return failed(capture,NULL);}
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return failed(capture,NULL);
    xfuture* source=xrtNetStreamRecvAsync(scope.stream,count);xl6ExecutionTcpLeaveInternalV1(&scope);
    if(!source)return failed(capture,NULL);
    xfuture* result=xl6FutureCallbackRuntimeV6Get()->StartExclusive(services.executor,capture,source);
    if(!result)return failed(capture,source);
    xrtFutureDestroy(source);return result;
}
