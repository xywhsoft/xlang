#include "xl6_net_file_internal.h"
#include "xl6_future_callback.h"
#include "xl6_future_payload.h"

typedef struct XL6_NetFileCaptureV1 {xvalue* receiver;XL6_NetFileResultV1 mode;} Capture;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(Capture,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Capture,mode),sizeof(XL6_NetFileResultV1)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Capture),2,slots};
static xl6_error_result discard(void** box,XL6_FuturePayloadKind kind,xl6_error_result outcome)
{
    if(!*box)return outcome;
    if(kind==XL6_FUTURE_PAYLOAD_XVALUE){
        static const XL6_CaptureSlotV2 slot={{XL6_CAPTURE_XVALUE,0,sizeof(xvalue*)},NULL};
        static const XL6_CaptureLayoutV2 valueLayout={sizeof(valueLayout),sizeof(xvalue*),1,&slot};
        outcome=xl6CaptureRuntimeV2Get()->Cleanup(&valueLayout,*box,sizeof(xvalue*),outcome);
    }
    xrtFree(*box);*box=NULL;return outcome;
}
static void completed(const xfutureresult* input,xpromise* output,xcancel* cancel,void* raw)
{
    Capture* capture=raw;XL6_NetFileResultV1 mode=capture->mode;
    XL6_FuturePayloadKind kind=mode==XL6_NET_FILE_READ?XL6_FUTURE_PAYLOAD_XVALUE:XL6_FUTURE_PAYLOAD_VALUE;
    void* box=NULL;bool cancelled=false;xl6_error_result outcome={true,NULL};
    if(!input)outcome=(xl6_error_result){false,xrtTakeError()};
    else if(xrtCancelRequested(cancel)||input->State==XFUTURE_CANCELLED)cancelled=true;
    else if(input->State==XFUTURE_FAILED)outcome=(xl6_error_result){false,xrtErrorRef(input->Error)};
    else if(input->State!=XFUTURE_RESOLVED){xrtSetErrorKind(XERR_STATE);outcome=(xl6_error_result){false,xrtTakeError()};}
    else if(mode!=XL6_NET_FILE_CLOSE){
        box=xrtCalloc(1,mode==XL6_NET_FILE_READ?sizeof(xvalue*):sizeof(int64_t));
        bool ok=box!=NULL;
        if(ok&&mode==XL6_NET_FILE_READ){
            xbuffer* buffer=input->Value;
            if(!buffer||buffer->Size>INT64_MAX){xrtSetErrorKind(XERR_STATE);ok=false;}
            else{
                /* This source is private to ONE resident mapping. Move its
                 * owned allocation once, then cache one immutable bytes value
                 * in the language Future for every await/erased observer. */
                size_t count=0;bytes data=xrtBufferTake(buffer,&count,NULL);
                *(xvalue**)box=xrtValueBytesTake(&data,count);xrtFree(data);ok=*(xvalue**)box!=NULL;
            }
        }else if(ok){
            uintptr_t count=(uintptr_t)input->Value;
            if(count>INT64_MAX){xrtSetErrorKind(XERR_RANGE);ok=false;}else *(int64_t*)box=(int64_t)count;
        }
        if(!ok)outcome=(xl6_error_result){false,xrtTakeError()};
    }
    outcome=xl6FutureCallbackRuntimeV5Get()->Cleanup(raw,outcome);
    if(!outcome.ok){outcome=discard(&box,kind,outcome);xl6FutureCallbackFailV1(output,outcome.error);return;}
    if(cancelled){(void)xrtPromiseCancel(output);return;}
    if(!xl6PayloadResolveOwned(output,box,kind)){
        outcome=discard(&box,kind,(xl6_error_result){false,xrtTakeError()});xl6FutureCallbackFailV1(output,outcome.error);
    }
}
Capture* xl6NetFilePrepareInternalV1(void* receiver,XL6_NetFileResultV1 mode)
{
    Capture* capture=xl6FutureCallbackRuntimeV5Get()->Allocate(NULL,sizeof(*capture),completed,&layout);
    if(!capture)return NULL;
    *capture=(Capture){.mode=mode};
    void* owner=xl6ExecutionResourceRetainV1(receiver);
    /* Bind the exact phased trace as well as Drop. An unbound raw Handle
     * cannot be admitted when an unobserved frame reaches module retirement. */
    if(owner)capture->receiver=xl6NativeHandleTakeV1(&owner,xl6NetFileHandlePolicyV1Get());
    if(owner)xl6ExecutionResourceReleaseV1(owner);
    if(!capture->receiver){(void)xl6NetFileFailedInternalV1(capture,NULL);return NULL;}
    return capture;
}
xfuture* xl6NetFileFailedInternalV1(Capture* capture,xfuture* source)
{
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    if(source&&(!capture||capture->mode!=XL6_NET_FILE_CLOSE)&&xrtFutureState(source)==XFUTURE_PENDING)
        (void)xrtFutureCancel(source);
    xl6FutureCallbackRuntimeV1Get()->Discard(capture);xrtFutureDestroy(source);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);xl6ErrorScopePropagateFailure(outcome);return NULL;
}
xfuture* xl6NetFilePublishInternalV1(XL6_RuntimeServicesV1 services,Capture* capture,xfuture* source)
{
    if(!source)return xl6NetFileFailedInternalV1(capture,NULL);
    /* Close observers share the one physical Close Future. Cancelling one
     * language observer must never cancel other observers or native cleanup. */
    xfuture* result=capture->mode==XL6_NET_FILE_CLOSE?
        xl6FutureCallbackRuntimeV5Get()->Start(services.executor,capture,source):
        xl6FutureCallbackRuntimeV6Get()->StartExclusive(services.executor,capture,source);
    if(!result)return xl6NetFileFailedInternalV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6NetFileReadCancelV1(XL6_RuntimeServicesV1 services,void* file,uint64_t offset,int64_t count,void* cancel)
{
    if(count<0||(uint64_t)count>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);return xl6NetFileFailedInternalV1(NULL,NULL);}
    Capture* capture=xl6NetFilePrepareInternalV1(file,XL6_NET_FILE_READ);
    if(!capture)return NULL;
    return xl6NetFilePublishInternalV1(services,capture,xl6ExecutionNetFileReadV1(file,offset,(size_t)count,cancel));
}
xfuture* xl6NetFileReadV1(XL6_RuntimeServicesV1 services,void* file,uint64_t offset,int64_t count)
{return xl6NetFileReadCancelV1(services,file,offset,count,NULL);}
xfuture* xl6NetFileCloseV1(XL6_RuntimeServicesV1 services,void* file)
{
    Capture* capture=xl6NetFilePrepareInternalV1(file,XL6_NET_FILE_CLOSE);if(!capture)return NULL;
    return xl6NetFilePublishInternalV1(services,capture,xl6ExecutionNetFileCloseSourceV1(file));
}
