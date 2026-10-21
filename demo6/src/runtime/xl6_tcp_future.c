#include "xl6_tcp_internal.h"
#include "xl6_native_class_borrow.h"
#include "xl6_future_callback.h"
#include "xl6_future_payload.h"
#include "xl6_tcp_future_internal.h"
typedef enum Mode {RECEIVE,WAIT,CLASS_RESULT} Mode;
typedef struct XL6_TcpClassCaptureV1 {
    xvalue *receiver,*object,*resultHandle;
    xstrview field;
    Mode mode;
    XL6_TcpPromoteResidentV1 promote;
} Capture;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(Capture,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_XVALUE,offsetof(Capture,object),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_XVALUE,offsetof(Capture,resultHandle),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_STRING,offsetof(Capture,field),sizeof(xstrview)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,mode),sizeof(Mode)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,promote),sizeof(XL6_TcpPromoteResidentV1)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Capture),sizeof(slots)/sizeof(slots[0]),slots};
static xvalue* resourceHandle(void* resource,const XL6_NativeHandlePolicyV1* policy)
{
    void* owned=xl6ExecutionResourceRetainV1(resource);if(!owned)return NULL;
    xvalue* result=xrtValueHandleTake(&owned,policy->Ops,NULL);
    if(!result)xl6ExecutionResourceReleaseV1(owned);
    return result;
}
static bool rawHandle(const xvalue* value,const XL6_NativeHandlePolicyV1* policy,void** output)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=policy->Ops){xrtSetErrorKind(XERR_TYPE);return false;}
    *output=raw;return true;
}
static xl6_error_result discardBox(void** box,XL6_FuturePayloadKind kind,xl6_error_result incoming)
{
    if(!*box)return incoming;
    if(kind==XL6_FUTURE_PAYLOAD_XVALUE){
        static const XL6_CaptureSlotV2 valueSlot={{XL6_CAPTURE_XVALUE,0,sizeof(xvalue*)},NULL};
        static const XL6_CaptureLayoutV2 valueLayout={sizeof(valueLayout),sizeof(xvalue*),1,&valueSlot};
        incoming=xl6CaptureRuntimeV2Get()->Cleanup(&valueLayout,*box,sizeof(xvalue*),incoming);
    }
    xrtFree(*box);*box=NULL;return incoming;
}
static bool acceptValue(Capture* capture,xvalue** output)
{
    void* resource=NULL;
    if(xrtValueObjectGet(capture->object,capture->field)!=capture->resultHandle||
        !rawHandle(capture->resultHandle,xl6TcpStreamHandlePolicyV1Get(),&resource)){
        if(!xrtGetError())xrtSetErrorKind(XERR_STATE);
        return false;
    }
    if(capture->promote&&!capture->promote(resource))return false;
    *output=xrtValueRetain(capture->object);return *output!=NULL;
}
static void completed(const xfutureresult* input,xpromise* output,xcancel* cancel,void* raw)
{
    Capture* capture=raw;Mode mode=capture->mode;void* box=NULL;
    XL6_FuturePayloadKind kind=mode==WAIT?XL6_FUTURE_PAYLOAD_VALUE:XL6_FUTURE_PAYLOAD_XVALUE;
    xfuturestate terminal=XFUTURE_RESOLVED;xl6_error_result outcome={true,NULL};
    if(!input){outcome=(xl6_error_result){false,xrtTakeError()};}
    else if(xrtCancelRequested(cancel)||input->State==XFUTURE_CANCELLED)terminal=XFUTURE_CANCELLED;
    else if(input->State==XFUTURE_FAILED){outcome=(xl6_error_result){false,xrtErrorRef(input->Error)};}
    else if(input->State==XFUTURE_CLOSED&&mode==CLASS_RESULT)terminal=XFUTURE_CLOSED;
    else if(input->State!=XFUTURE_RESOLVED&&input->State!=XFUTURE_CLOSED){
        xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};
    }else{
        box=xrtCalloc(1,mode==WAIT?sizeof(bool):sizeof(xvalue*));
        bool ok=box!=NULL;
        if(ok&&mode==WAIT)*(bool*)box=input->State==XFUTURE_RESOLVED;
        else if(ok&&mode==CLASS_RESULT)ok=acceptValue(capture,box);
        else if(ok){
            if(input->State==XFUTURE_RESOLVED&&!input->Value){xrtSetErrorKind(XERR_STATE);ok=false;}
            else{ /* One direct native-view -> language bytes copy, no Buffer or C string. */
                xbytesview bytes=input->State==XFUTURE_CLOSED?(xbytesview){0}:xrtNetBytesView(input->Value);
                *(xvalue**)box=xrtValueBytes(bytes);ok=*(xvalue**)box!=NULL;
            }
        }
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
static Capture* prepare(Mode mode,void* receiver,const XL6_NativeHandlePolicyV1* policy)
{
    Capture* capture=xl6FutureCallbackRuntimeV5Get()->Allocate(NULL,sizeof(*capture),completed,&layout);
    if(!capture)return NULL;
    *capture=(Capture){.mode=mode};capture->receiver=resourceHandle(receiver,policy);
    return capture;
}
static xfuture* failed(Capture* capture,xfuture* source)
{
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    if(source&&xrtFutureState(source)==XFUTURE_PENDING)(void)xrtFutureCancel(source);
    xl6FutureCallbackRuntimeV1Get()->Discard(capture);xrtFutureDestroy(source);
    xl6_error_result result=xl6ErrorScopeEnd(&cleanup);xl6ErrorScopePropagateFailure(result);return NULL;
}
static xfuture* publish(XL6_RuntimeServicesV1 services,Capture* capture,xfuture* source)
{
    if(!source)return failed(capture,NULL);
    xfuture* result=xl6FutureCallbackRuntimeV6Get()->StartExclusive(services.executor,capture,source);
    if(!result)return failed(capture,source);
    xrtFutureDestroy(source);return result;
}
XL6_TcpClassCaptureV1* xl6TcpClassCapturePrepareInternalV1(XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,const XL6_NativeHandlePolicyV1* receiverPolicy,
    XL6_TcpPromoteResidentV1 promote,bool same,XL6_ExecutionResourceV1** output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);(void)failed(NULL,NULL);return NULL;}*output=NULL;
    Capture* capture=prepare(CLASS_RESULT,receiver,receiverPolicy);
    if(!capture||!capture->receiver){(void)failed(capture,NULL);return NULL;}
    capture->promote=promote;
    void* carrier=NULL;
    capture->resultHandle=xl6NativeClassFieldRetainV1(context,object,xl6TcpStreamHandlePolicyV1Get(),&carrier);
    if(!capture->resultHandle){(void)failed(capture,NULL);return NULL;}
    if(same&&carrier!=receiver){xrtSetErrorKind(XERR_STATE);(void)failed(capture,NULL);return NULL;}
    capture->object=xrtValueRetain(object);if(!capture->object){(void)failed(capture,NULL);return NULL;}
    char* field=xrtMalloc(context.fieldSize);if(!field){(void)failed(capture,NULL);return NULL;}
    memcpy(field,context.fieldName,context.fieldSize);capture->field=(xstrview){field,context.fieldSize};
    *output=carrier;return capture;
}
xfuture* xl6TcpClassCapturePublishInternalV1(XL6_RuntimeServicesV1 services,XL6_TcpClassCaptureV1* capture,xfuture* source)
{return publish(services,capture,source);}
xfuture* xl6TcpClassCaptureFailInternalV1(XL6_TcpClassCaptureV1* capture,xfuture* source)
{return failed(capture,source);}
static xfuture* classFuture(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,bool accept)
{
    XL6_ExecutionResourceV1* carrier=NULL;
    Capture* capture=xl6TcpClassCapturePrepareInternalV1(context,receiver,object,
        accept?xl6TcpListenerHandlePolicyV1Get():xl6TcpStreamHandlePolicyV1Get(),
        accept?xl6ExecutionTcpPromoteAcceptV1:NULL,!accept,&carrier);
    if(!capture)return NULL;
    if(accept)return publish(services,capture,xl6ExecutionTcpAcceptSourceForV1(carrier,receiver));
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return failed(capture,NULL);
    xfuture* source=xrtNetStreamWaitAsync(scope.stream,XNET_STREAM_WAIT_OPEN);xl6ExecutionTcpLeaveInternalV1(&scope);
    return publish(services,capture,source);
}
xfuture* xl6TcpAcceptStartV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* listener,const xvalue* object)
{return classFuture(services,context,listener,object,true);}
xfuture* xl6TcpConnectedStartV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* stream,const xvalue* object)
{return classFuture(services,context,stream,object,false);}
static xfuture* streamFuture(XL6_RuntimeServicesV1 services,void* receiver,int64_t argument,unsigned operation)
{
    size_t bytes=0;
    if(operation==1){if(argument<XNET_STREAM_WAIT_OPEN||argument>XNET_STREAM_WAIT_CLOSE){xrtSetErrorKind(XERR_RANGE);return failed(NULL,NULL);}}
    else if(!xl6TcpPositiveSizeV1(argument,&bytes))return failed(NULL,NULL);
    Capture* capture=prepare(operation==0?RECEIVE:WAIT,receiver,xl6TcpStreamHandlePolicyV1Get());
    if(!capture||!capture->receiver)return failed(capture,NULL);
    /* Waiting observes transport state, including explicit Close. Only Receive
     * submits new data work. The retained capture keeps the native child alive. */
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,operation==0,&scope))return failed(capture,NULL);
    xfuture* source=operation==0?xrtNetStreamRecvAsync(scope.stream,bytes):operation==1?
        xrtNetStreamWaitAsync(scope.stream,(xnetstreamwait)argument):xrtNetStreamWaitAvailableAsync(scope.stream,bytes);
    xl6ExecutionTcpLeaveInternalV1(&scope);return publish(services,capture,source);
}
xfuture* xl6TcpStreamRecvAsyncV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t maximum)
{return streamFuture(services,receiver,maximum,0);}
xfuture* xl6TcpStreamWaitAsyncV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t condition)
{return streamFuture(services,receiver,condition,1);}
xfuture* xl6TcpStreamWaitAvailableAsyncV1(XL6_RuntimeServicesV1 services,void* receiver,int64_t minimum)
{return streamFuture(services,receiver,minimum,2);}
