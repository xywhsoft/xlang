#include "xl6_tls_io_internal.h"
#include "xl6_future_callback_internal.h"

typedef struct Capture {
    xvalue *receiver,*carrier;
    xstrview field;
    const XL6_ClassIdentitySchemaV3* schema;
} Capture;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(Capture,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_XVALUE,offsetof(Capture,carrier),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_STRING,offsetof(Capture,field),sizeof(xstrview)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,schema),sizeof(void*)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Capture),4,slots};
static xvalue* delivered(Capture* c)
{
    void *raw=NULL,*user=NULL;const xvaluehandleops* ops=NULL;
    if(!xrtValueGetHandle(c->carrier,&raw,&ops,&user)||!raw||user||ops!=xl6TlsStreamHandlePolicyV1Get()->Ops){
        xrtSetErrorKind(XERR_STATE);return NULL;}
    XL6_NativeClassResultContextV1 context={{c->field.Data,c->field.Size,xl6TlsStreamHandlePolicyV1Get()},NULL,c->schema};
    xvalue* object=xl6NativeClassResultPrepareFromLeaseV1(context,xl6CallbackFrameHeaderV1(c)->lease);
    if(!object)return NULL;
    void* owned=xl6ExecutionTlsPromoteAcceptV1(raw)?xl6ExecutionResourceRetainV1(raw):NULL;
    if(!owned){
        xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
        xrtValueRelease(object);xl6_error_result error=xl6ErrorScopeEnd(&cleanup);xrtSetErrorTake(error.error);return NULL;
    }
    return xl6NativeClassResultCommitTakeV1(context,&object,&owned);
}
static xl6_error_result discard(xvalue** box,xl6_error_result outcome)
{
    if(box){
        static const XL6_CaptureSlotV2 slot={{XL6_CAPTURE_XVALUE,0,sizeof(xvalue*)},NULL};
        static const XL6_CaptureLayoutV2 value={sizeof(value),sizeof(xvalue*),1,&slot};
        outcome=xl6CaptureRuntimeV2Get()->Cleanup(&value,box,sizeof(*box),outcome);xrtFree(box);
    }return outcome;
}
static void completed(const xfutureresult* input,xpromise* output,xcancel* cancel,void* raw)
{
    xvalue** box=NULL;xfuturestate terminal=XFUTURE_RESOLVED;xl6_error_result outcome={true,NULL};
    if(!input){xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};}
    else if(input->State==XFUTURE_CANCELLED||xrtCancelRequested(cancel))terminal=XFUTURE_CANCELLED;
    else if(input->State==XFUTURE_FAILED)outcome=(xl6_error_result){false,xrtErrorRef(input->Error)};
    else if(input->State==XFUTURE_CLOSED)terminal=XFUTURE_CLOSED;
    else if(input->State!=XFUTURE_RESOLVED){xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};}
    else{
        box=xrtCalloc(1,sizeof(*box));if(box)*box=delivered(raw);
        if(!box||!*box)outcome=(xl6_error_result){false,xrtTakeError()};
    }
    outcome=xl6FutureCallbackRuntimeV5Get()->Cleanup(raw,outcome);
    if(!outcome.ok){outcome=discard(box,outcome);xl6FutureCallbackFailV1(output,outcome.error);return;}
    if(terminal==XFUTURE_CANCELLED){(void)xrtPromiseCancel(output);return;}
    if(terminal==XFUTURE_CLOSED){(void)xrtPromiseClose(output);return;}
    if(!xl6PayloadResolveOwned(output,box,XL6_FUTURE_PAYLOAD_XVALUE)){
        outcome=discard(box,(xl6_error_result){false,xrtTakeError()});xl6FutureCallbackFailV1(output,outcome.error);
    }
}
void* xl6TlsAcceptFuturePrepareV1(XL6_NativeClassResultContextV1 context,void* receiver)
{
    if(context.field.policy!=xl6TlsStreamHandlePolicyV1Get()){
        xrtSetErrorKind(XERR_ARGUMENT);return xl6TlsFutureFailV1(NULL,NULL);}
    if(!xl6NativeClassResultContextValidV1(context))return xl6TlsFutureFailV1(NULL,NULL);
    Capture* c=xl6FutureCallbackRuntimeV5Get()->Allocate(context.codeOwner,sizeof(*c),completed,&layout);
    if(!c)return xl6TlsFutureFailV1(NULL,NULL);
    *c=(Capture){.schema=context.schema};
    void* owned=xl6ExecutionResourceRetainV1(receiver);
    if(owned)c->receiver=xl6NativeHandleTakeV1(&owned,xl6TlsListenerHandlePolicyV1Get());
    if(!c->receiver)return xl6TlsFutureFailV1(c,NULL);
    char* field=xrtMalloc(context.field.fieldSize);if(!field)return xl6TlsFutureFailV1(c,NULL);
    memcpy(field,context.field.fieldName,context.field.fieldSize);c->field=(xstrview){field,context.field.fieldSize};
    return c;
}
xfuture* xl6TlsAcceptSourceV1(XL6_ExecutionServicesV1 execution,void* receiver,void* raw)
{
    Capture* c=raw;
    if(!c||c->carrier||!execution.source){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    void* carrier=xl6ExecutionTlsPrepareAcceptV1(execution.source,receiver);
    if(!carrier)return NULL;
    void* borrowed=carrier;c->carrier=xl6NativeHandleTakeV1(&carrier,xl6TlsStreamHandlePolicyV1Get());
    if(!c->carrier)return NULL;
    return xl6ExecutionTlsAcceptSourceForV1(borrowed,receiver);
}
