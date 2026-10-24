#include "xl6_tls_io_internal.h"

typedef struct Capture {xvalue* receiver;XL6_TlsIoKindV1 kind;int64_t count;} Capture;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(Capture,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,kind),sizeof(XL6_TlsIoKindV1)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,count),sizeof(int64_t)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Capture),3,slots};
static xl6_error_result discard(void* box,XL6_FuturePayloadKind kind,xl6_error_result outcome)
{
    if(box&&kind==XL6_FUTURE_PAYLOAD_XVALUE){
        static const XL6_CaptureSlotV2 slot={{XL6_CAPTURE_XVALUE,0,sizeof(xvalue*)},NULL};
        static const XL6_CaptureLayoutV2 value={sizeof(value),sizeof(xvalue*),1,&slot};
        outcome=xl6CaptureRuntimeV2Get()->Cleanup(&value,box,sizeof(xvalue*),outcome);
    }
    xrtFree(box);return outcome;
}
static void completed(const xfutureresult* input,xpromise* output,xcancel* cancel,void* raw)
{
    Capture* c=raw;XL6_TlsIoKindV1 operation=c->kind;void* box=NULL;
    XL6_FuturePayloadKind kind=operation==XL6_TLS_IO_RECEIVE_V1?XL6_FUTURE_PAYLOAD_XVALUE:XL6_FUTURE_PAYLOAD_VALUE;
    xfuturestate terminal=XFUTURE_RESOLVED;xl6_error_result outcome={true,NULL};
    if(!input){xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};}
    else if(input->State==XFUTURE_CANCELLED||xrtCancelRequested(cancel))terminal=XFUTURE_CANCELLED;
    else if(input->State==XFUTURE_FAILED)outcome=(xl6_error_result){false,xrtErrorRef(input->Error)};
    else if(input->State==XFUTURE_CLOSED&&operation==XL6_TLS_IO_SEND_V1)terminal=XFUTURE_CLOSED;
    else if(input->State!=XFUTURE_RESOLVED&&input->State!=XFUTURE_CLOSED){
        xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};
    }else{
        size_t bytes=operation==XL6_TLS_IO_RECEIVE_V1?sizeof(xvalue*):operation==XL6_TLS_IO_WAIT_V1?sizeof(bool):sizeof(int64_t);
        box=xrtCalloc(1,bytes);bool ok=box!=NULL;
        if(ok&&operation==XL6_TLS_IO_WAIT_V1)*(bool*)box=input->State==XFUTURE_RESOLVED;
        else if(ok&&operation==XL6_TLS_IO_SEND_V1)*(int64_t*)box=c->count;
        else if(ok){
            if(input->State==XFUTURE_RESOLVED&&!input->Value){xrtSetErrorKind(XERR_STATE);ok=false;}
            else{*(xvalue**)box=xrtValueBytes(input->State==XFUTURE_CLOSED?(xbytesview){0}:xrtNetBytesView(input->Value));ok=*(xvalue**)box!=NULL;}
        }
        if(!ok)outcome=(xl6_error_result){false,xrtTakeError()};
    }
    outcome=xl6FutureCallbackRuntimeV5Get()->Cleanup(raw,outcome);
    if(!outcome.ok){outcome=discard(box,kind,outcome);xl6FutureCallbackFailV1(output,outcome.error);return;}
    if(terminal==XFUTURE_CANCELLED){(void)xrtPromiseCancel(output);return;}
    if(terminal==XFUTURE_CLOSED){(void)xrtPromiseClose(output);return;}
    if(!xl6PayloadResolveOwned(output,box,kind)){
        outcome=discard(box,kind,(xl6_error_result){false,xrtTakeError()});xl6FutureCallbackFailV1(output,outcome.error);
    }
}
void* xl6TlsIoFuturePrepareV1(XL6_TlsIoKindV1 kind,void* receiver,int64_t argument,xbytesview data)
{
    if(!xl6TlsIoArgumentV1(kind,argument)||data.Size>INT64_MAX){
        if(data.Size>INT64_MAX)xrtSetErrorKind(XERR_RANGE);
        return xl6TlsFutureFailV1(NULL,NULL);}
    Capture* c=xl6FutureCallbackRuntimeV5Get()->Allocate(NULL,sizeof(*c),completed,&layout);
    if(!c)return xl6TlsFutureFailV1(NULL,NULL);
    *c=(Capture){.kind=kind,.count=(int64_t)data.Size};
    void* owned=xl6ExecutionResourceRetainV1(receiver);
    if(owned)c->receiver=xl6NativeHandleTakeV1(&owned,xl6TlsStreamHandlePolicyV1Get());
    if(!c->receiver)return xl6TlsFutureFailV1(c,NULL);
    return c;
}
bool xl6TlsIoFutureSetSendSizeV1(void* raw,size_t count)
{
    Capture* capture=raw;
    if(!capture||capture->kind!=XL6_TLS_IO_SEND_V1||count>INT64_MAX){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    capture->count=(int64_t)count;return true;
}
