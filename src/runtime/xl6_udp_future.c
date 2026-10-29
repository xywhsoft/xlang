#include "xl6_udp_future_internal.h"
#include "xl6_future_payload.h"
#include "xl6_future_callback_internal.h"
typedef struct XL6_UdpFutureCaptureV1 {
    xvalue* receiver;
    xstrview field;
    const XL6_ClassIdentitySchemaV3* schema;
    XL6_UdpFutureKindV1 kind;
} Capture;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(Capture,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_STRING,offsetof(Capture,field),sizeof(xstrview)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,schema),sizeof(void*)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,kind),sizeof(XL6_UdpFutureKindV1)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Capture),sizeof(slots)/sizeof(*slots),slots};
static const XL6_NativeHandlePolicyV1* policy(XL6_UdpFutureKindV1 kind)
{return kind==XL6_UDP_FUTURE_PACKET?xl6UdpPacketHandlePolicyV1Get():kind==XL6_UDP_FUTURE_ERROR?
    xl6UdpErrorPacketHandlePolicyV1Get():kind==XL6_UDP_FUTURE_BATCH?xl6UdpBatchHandlePolicyV1Get():NULL;}
static xl6_error_result discardBox(void** box,XL6_FuturePayloadKind kind,xl6_error_result outcome)
{
    if(!*box)return outcome;
    if(kind==XL6_FUTURE_PAYLOAD_XVALUE){
        static const XL6_CaptureSlotV2 slot={{XL6_CAPTURE_XVALUE,0,sizeof(xvalue*)},NULL};
        static const XL6_CaptureLayoutV2 value={sizeof(value),sizeof(xvalue*),1,&slot};
        outcome=xl6CaptureRuntimeV2Get()->Cleanup(&value,*box,sizeof(xvalue*),outcome);
    }xrtFree(*box);*box=NULL;return outcome;
}
static xvalue* classResult(Capture* capture,void* input)
{
    if(!input){xrtSetErrorKind(XERR_STATE);return NULL;}
    XL6_NativeClassResultContextV1 context={{capture->field.Data,capture->field.Size,policy(capture->kind)},NULL,capture->schema};
    XL6_FutureCallbackFrameV1* frame=xl6CallbackFrameHeaderV1(capture);
    xvalue* prepared=xl6NativeClassResultPrepareFromLeaseV1(context,frame->lease);if(!prepared)return NULL;
    void* owned=capture->kind==XL6_UDP_FUTURE_PACKET?(void*)xrtNetUdpPacketRef(input):
        capture->kind==XL6_UDP_FUTURE_ERROR?(void*)xrtNetUdpErrorPacketRef(input):(void*)xrtNetUdpBatchRef(input);
    if(!owned){
        xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
        xrtValueRelease(prepared);xl6_error_result result=xl6ErrorScopeEnd(&cleanup);xrtSetErrorTake(result.error);return NULL;
    }
    return xl6NativeClassResultCommitTakeV1(context,&prepared,&owned);
}
static void completed(const xfutureresult* input,xpromise* output,xcancel* cancel,void* raw)
{
    Capture* capture=raw;bool value=capture->kind>=XL6_UDP_FUTURE_WAIT;
    XL6_FuturePayloadKind kind=value?XL6_FUTURE_PAYLOAD_VALUE:XL6_FUTURE_PAYLOAD_XVALUE;
    xfuturestate terminal=XFUTURE_RESOLVED;xl6_error_result outcome={true,NULL};void* box=NULL;
    if(!input)outcome=(xl6_error_result){false,xrtTakeError()};
    else if(input->State==XFUTURE_CANCELLED||xrtCancelRequested(cancel))terminal=XFUTURE_CANCELLED;
    else if(input->State==XFUTURE_FAILED)outcome=(xl6_error_result){false,xrtErrorRef(input->Error)};
    else if(input->State==XFUTURE_CLOSED&&!value)terminal=XFUTURE_CLOSED;
    else if(input->State!=XFUTURE_RESOLVED&&input->State!=XFUTURE_CLOSED){
        xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};
    }else{
        box=xrtCalloc(1,value?sizeof(bool):sizeof(xvalue*));bool ok=box!=NULL;
        if(ok){if(value)*(bool*)box=input->State==XFUTURE_RESOLVED;
            else{*(xvalue**)box=classResult(capture,input->Value);ok=*(xvalue**)box!=NULL;}}
        if(!ok)outcome=(xl6_error_result){false,xrtTakeError()};
    }
    outcome=xl6FutureCallbackRuntimeV5Get()->Cleanup(raw,outcome);
    if(!outcome.ok){outcome=discardBox(&box,kind,outcome);xl6FutureCallbackFailV1(output,outcome.error);return;}
    if(terminal==XFUTURE_CANCELLED){(void)xrtPromiseCancel(output);return;}
    if(terminal==XFUTURE_CLOSED){(void)xrtPromiseClose(output);return;}
    if(!xl6PayloadResolveOwned(output,box,kind)){
        outcome=discardBox(&box,kind,(xl6_error_result){false,xrtTakeError()});xl6FutureCallbackFailV1(output,outcome.error);
    }
}
xfuture* xl6UdpFutureFailInternalV1(Capture* capture,xfuture* source)
{
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    if(source&&xrtFutureState(source)==XFUTURE_PENDING)(void)xrtFutureCancel(source);
    xl6FutureCallbackRuntimeV1Get()->Discard(capture);xrtFutureDestroy(source);
    xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&cleanup));return NULL;
}
Capture* xl6UdpFuturePrepareInternalV1(XL6_UdpFutureKindV1 kind,void* receiver,int64_t argument,const XL6_NativeClassResultContextV1* context)
{
    if((unsigned)kind>XL6_UDP_FUTURE_WRITABLE||
        (kind==XL6_UDP_FUTURE_BATCH&&(argument<1||argument>256))||
        (kind==XL6_UDP_FUTURE_WAIT&&(argument<XNET_UDP_WAIT_OPEN||argument>XNET_UDP_WAIT_CLOSE))||
        (kind==XL6_UDP_FUTURE_WRITABLE&&(argument<0||(uint64_t)argument>SIZE_MAX))||
        (kind<XL6_UDP_FUTURE_WAIT&&(!context||context->field.policy!=policy(kind)))){
        xrtSetErrorKind(XERR_RANGE);(void)xl6UdpFutureFailInternalV1(NULL,NULL);return NULL;
    }
    if(kind<XL6_UDP_FUTURE_WAIT&&!xl6NativeClassResultContextValidV1(*context)){
        (void)xl6UdpFutureFailInternalV1(NULL,NULL);return NULL;
    }
    Capture* capture=xl6FutureCallbackRuntimeV5Get()->Allocate(context?context->codeOwner:NULL,sizeof(*capture),completed,&layout);
    if(!capture){(void)xl6UdpFutureFailInternalV1(NULL,NULL);return NULL;}
    *capture=(Capture){.kind=kind};void* owner=xl6ExecutionResourceRetainV1(receiver);
    if(owner)capture->receiver=xl6NativeHandleTakeV1(&owner,xl6UdpHandlePolicyV1Get());
    if(!capture->receiver)goto failed;
    if(kind<XL6_UDP_FUTURE_WAIT){
        char* field=xrtMalloc(context->field.fieldSize);if(!field)goto failed;
        memcpy(field,context->field.fieldName,context->field.fieldSize);capture->field=(xstrview){field,context->field.fieldSize};
        capture->schema=context->schema;
    }return capture;
failed:(void)xl6UdpFutureFailInternalV1(capture,NULL);return NULL;
}
xfuture* xl6UdpFutureSourceInternalV1(XL6_UdpFutureKindV1 kind,void* receiver,int64_t argument)
{
    switch(kind){
        case XL6_UDP_FUTURE_PACKET:return xl6ExecutionUdpReceiveAsyncV1(receiver);
        case XL6_UDP_FUTURE_ERROR:return xl6ExecutionUdpReceiveErrorAsyncV1(receiver);
        case XL6_UDP_FUTURE_BATCH:return xl6ExecutionUdpReceiveBatchAsyncV1(receiver,(size_t)argument);
        case XL6_UDP_FUTURE_WAIT:return xl6ExecutionUdpWaitAsyncV1(receiver,(int)argument);
        case XL6_UDP_FUTURE_WRITABLE:return xl6ExecutionUdpWritableAsyncV1(receiver,(size_t)argument);
    }xrtSetErrorKind(XERR_ARGUMENT);return NULL;
}
xfuture* xl6UdpFuturePublishInternalV1(XL6_RuntimeServicesV1 services,Capture* capture,xfuture* source)
{
    if(!source)return xl6UdpFutureFailInternalV1(capture,NULL);
    xfuture* result=xl6FutureCallbackRuntimeV6Get()->StartExclusive(services.executor,capture,source);
    if(!result)return xl6UdpFutureFailInternalV1(capture,source);
    xrtFutureDestroy(source);return result;
}
static xfuture* start(XL6_RuntimeServicesV1 services,XL6_UdpFutureKindV1 kind,void* receiver,int64_t argument,const XL6_NativeClassResultContextV1* context)
{
    if(!services.executor){xrtSetErrorKind(XERR_ARGUMENT);return xl6UdpFutureFailInternalV1(NULL,NULL);}
    Capture* capture=xl6UdpFuturePrepareInternalV1(kind,receiver,argument,context);
    return capture?xl6UdpFuturePublishInternalV1(services,capture,xl6UdpFutureSourceInternalV1(kind,receiver,argument)):NULL;
}
xfuture* xl6UdpReceiveAsyncV1(XL6_RuntimeServicesV1 s,XL6_NativeClassResultContextV1 c,void* r){return start(s,XL6_UDP_FUTURE_PACKET,r,0,&c);}
xfuture* xl6UdpReceiveErrorAsyncV1(XL6_RuntimeServicesV1 s,XL6_NativeClassResultContextV1 c,void* r){return start(s,XL6_UDP_FUTURE_ERROR,r,0,&c);}
xfuture* xl6UdpReceiveBatchAsyncV1(XL6_RuntimeServicesV1 s,XL6_NativeClassResultContextV1 c,void* r,int64_t n){return start(s,XL6_UDP_FUTURE_BATCH,r,n,&c);}
xfuture* xl6UdpWaitAsyncV1(XL6_RuntimeServicesV1 s,void* r,int64_t n){return start(s,XL6_UDP_FUTURE_WAIT,r,n,NULL);}
xfuture* xl6UdpWaitWritableAsyncV1(XL6_RuntimeServicesV1 s,void* r,int64_t n){return start(s,XL6_UDP_FUTURE_WRITABLE,r,n,NULL);}
