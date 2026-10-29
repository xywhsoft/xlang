#include "xl6_tls_data_internal.h"
#include "xl6_future_callback_internal.h"

typedef struct Capture {xvalue* receiver;xstrview field;const XL6_ClassIdentitySchemaV3* schema;} Capture;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(Capture,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_STRING,offsetof(Capture,field),sizeof(xstrview)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,schema),sizeof(void*)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Capture),3,slots};
static xvalue* value(Capture* c,const xfutureresult* result)
{
    XL6_NativeClassResultContextV1 context={{c->field.Data,c->field.Size,xl6NetBytesHandlePolicyV1Get()},NULL,c->schema};
    xvalue* object=xl6NativeClassResultPrepareFromLeaseV1(context,xl6CallbackFrameHeaderV1(c)->lease);
    if(!object)return NULL;
    void* owned=result->State==XFUTURE_CLOSED?(void*)xrtNetBytesCopy(NULL,0):result->Value?(void*)xrtNetBytesRef(result->Value):NULL;
    if(!owned){
        if(!xrtGetError())xrtSetErrorKind(XERR_STATE);
        xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
        xrtValueRelease(object);xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);xrtSetErrorTake(outcome.error);return NULL;
    }
    return xl6NativeClassResultCommitTakeV1(context,&object,&owned);
}
static xl6_error_result discard(xvalue** box,xl6_error_result outcome)
{
    if(box){
        static const XL6_CaptureSlotV2 slot={{XL6_CAPTURE_XVALUE,0,sizeof(xvalue*)},NULL};
        static const XL6_CaptureLayoutV2 item={sizeof(item),sizeof(xvalue*),1,&slot};
        outcome=xl6CaptureRuntimeV2Get()->Cleanup(&item,box,sizeof(*box),outcome);xrtFree(box);
    }return outcome;
}
static void completed(const xfutureresult* input,xpromise* output,xcancel* cancel,void* raw)
{
    xvalue** box=NULL;xl6_error_result outcome={true,NULL};bool cancelled=false;
    if(!input){xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};}
    else if(input->State==XFUTURE_CANCELLED||xrtCancelRequested(cancel))cancelled=true;
    else if(input->State==XFUTURE_FAILED)outcome=(xl6_error_result){false,xrtErrorRef(input->Error)};
    else if(input->State!=XFUTURE_RESOLVED&&input->State!=XFUTURE_CLOSED){
        xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};
    }else{
        box=xrtCalloc(1,sizeof(*box));if(box)*box=value(raw,input);
        if(!box||!*box)outcome=(xl6_error_result){false,xrtTakeError()};
    }
    outcome=xl6FutureCallbackRuntimeV5Get()->Cleanup(raw,outcome);
    if(!outcome.ok){outcome=discard(box,outcome);xl6FutureCallbackFailV1(output,outcome.error);return;}
    if(cancelled){(void)xrtPromiseCancel(output);return;}
    if(!xl6PayloadResolveOwned(output,box,XL6_FUTURE_PAYLOAD_XVALUE)){
        outcome=discard(box,(xl6_error_result){false,xrtTakeError()});xl6FutureCallbackFailV1(output,outcome.error);
    }
}
void* xl6TlsRecvOwnedFuturePrepareV1(XL6_NativeClassResultContextV1 context,void* receiver,int64_t maximum)
{
    if(context.field.policy!=xl6NetBytesHandlePolicyV1Get()){
        xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!xl6TlsIoArgumentV1(XL6_TLS_IO_RECEIVE_V1,maximum)||!xl6NativeClassResultContextValidV1(context))return xl6TlsFutureFailV1(NULL,NULL);
    Capture* c=xl6FutureCallbackRuntimeV5Get()->Allocate(context.codeOwner,sizeof(*c),completed,&layout);
    if(!c)return xl6TlsFutureFailV1(NULL,NULL);
    *c=(Capture){.schema=context.schema};void* owned=xl6ExecutionResourceRetainV1(receiver);
    if(owned)c->receiver=xl6NativeHandleTakeV1(&owned,xl6TlsStreamHandlePolicyV1Get());
    if(!c->receiver)return xl6TlsFutureFailV1(c,NULL);
    char* field=xrtMalloc(context.field.fieldSize);if(!field)return xl6TlsFutureFailV1(c,NULL);
    memcpy(field,context.field.fieldName,context.field.fieldSize);c->field=(xstrview){field,context.field.fieldSize};return c;
}
