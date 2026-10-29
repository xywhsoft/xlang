#include "xl6_http_internal.h"
#include "xl6_tls_io_internal.h"
#include "xl6_task_frame_internal.h"
#include "xl6_task_net.h"

/* TLS owns its mutable plaintext queue on the engine worker. Host callers
 * submit one ordinary native task, never access that queue concurrently, and
 * never allocate a helper thread/stack. Engine-worker callers parse directly. */
typedef struct HttpTlsWork {xvalue* receiver;xhttp1limits limits;bool response;} HttpTlsWork;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(HttpTlsWork,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(HttpTlsWork,limits),sizeof(xhttp1limits)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(HttpTlsWork,response),sizeof(bool)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(HttpTlsWork),3,slots};
static XL6_HttpHead* parse(xtlsstream* stream,const xhttp1limits* limits,bool response)
{
    xhttpfield fields[100];xhttpfield* extra=NULL;xhttp1head head;xhttp1errorinfo error={0};
    xrtHttp1HeadInit(&head,fields,100);
    xhttp1status status=response?xrtHttp1ResponseParseTls(stream,&head,limits,&error):xrtHttp1RequestParseTls(stream,&head,limits,&error);
    if(status==XHTTP1_FIELDS){
        size_t count=head.FieldCount;
        if(count>SIZE_MAX/sizeof(xhttpfield))xrtSetErrorKind(XERR_RANGE);
        else extra=xrtMalloc(count*sizeof(*extra));
        if(extra){xrtHttp1HeadInit(&head,extra,count);status=response?xrtHttp1ResponseParseTls(stream,&head,limits,&error):xrtHttp1RequestParseTls(stream,&head,limits,&error);}
    }
    XL6_HttpHead* result=NULL;
    if(status==XHTTP1_READY){const xnetbuf* buffer=xrtTlsStreamBuffer(stream);xnetspan span={0};
        if(!buffer||!xrtNetBufFront(buffer,&span)||span.Size<head.Bytes)xrtSetErrorKind(XERR_INTERNAL);
        else result=xl6HttpHeadCopy(&head,(xbytesview){span.Data,span.Size});
    }else if(status==XHTTP1_ERROR&&error.Code)xl6HttpParseError(&error);
    xrtFree(extra);return result;
}
static XL6_TaskOutcome run(xcancel* cancel,void* raw,XL6_TaskResult* output)
{
    HttpTlsWork* work=raw;if(xrtCancelRequested(cancel))return XL6_TASK_CANCELLED;
    void *receiver=NULL,*user=NULL;const xvaluehandleops* ops=NULL;
    if(!xrtValueGetHandle(work->receiver,&receiver,&ops,&user)||!receiver||user||ops!=xl6TlsStreamHandlePolicyV1Get()->Ops){xrtSetErrorKind(XERR_TYPE);return XL6_TASK_FAILED;}
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsStreamEnterV1(receiver,false,&scope))return XL6_TASK_FAILED;
    void* owned=parse(scope.stream,&work->limits,work->response);bool ok=!xrtGetError();
    xvalue** box=ok?xrtCalloc(1,sizeof(*box)):NULL;
    *output=(XL6_TaskResult){box,box?xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE):NULL,NULL};
    if(box){*box=owned?xl6NativeHandleTakeV1(&owned,xl6Http1HeadHandlePolicyV2Get()):xrtValueNull();ok=*box!=NULL;}
    else ok=false;
    if(owned)xl6Http1HeadDestroyV2(owned);
    xl6ExecutionTlsLeaveV1(&scope);return ok?XL6_TASK_SUCCESS:XL6_TASK_FAILED;
}
static void* perform(void* receiver,const xhttp1limits* limits,bool response,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)||!xl6TlsIoPreWaitV1(due,NULL))return NULL;
    XL6_TlsScopeV1 stream={0};XL6_ExecutionScopeV1 engine={0};xfuture* future=NULL;
    HttpTlsWork* work=NULL;void* result=NULL;
    if(!xl6ExecutionTlsStreamEnterV1(receiver,false,&stream))return NULL;
    xnetstream* transport=xrtTlsStreamTransport(stream.stream);xnetworker* worker=transport?xrtNetStreamWorker(transport):NULL;
    if(!worker){xrtSetErrorKind(XERR_STATE);goto done;}
    if(xrtNetEngineCurrent(xrtNetWorkerEngine(worker))==worker){result=parse(stream.stream,limits,response);goto done;}
    if(!xl6TlsIoMayWaitV1(stream.engineOwner)||!xl6ExecutionResourceEnterV1(stream.engineOwner,false,&engine))goto done;
    work=xl6TaskRuntimeV3Get()->Allocate(NULL,sizeof(*work),run,&layout);if(!work)goto done;
    *work=(HttpTlsWork){.limits=*limits,.response=response};
    void* owned=xl6ExecutionResourceRetainV1(receiver);
    if(owned)work->receiver=xl6NativeHandleTakeV1(&owned,xl6TlsStreamHandlePolicyV1Get());
    if(!work->receiver)goto done;
    future=xl6TaskNetSubmitOwnedPolicyV1(engine.engine,xrtNetWorkerIndex(worker),work,NULL,xl6PayloadPolicyGet(XL6_FUTURE_PAYLOAD_XVALUE),false,0);
    if(!future)goto done;
    work=NULL;
    xfutureresult ready={0};
    if(xl6TlsIoWaitResultV1(future,due,NULL,&ready)){
        if(ready.State!=XFUTURE_RESOLVED||!ready.Value)xrtSetErrorKind(XERR_STATE);
        else{ xvalue* value=*(xvalue**)ready.Value;
            if(xrtValueType(value)!=XVALUE_NULL){
                void *raw=NULL,*user=NULL;const xvaluehandleops* ops=NULL;
                if(!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=xl6Http1HeadHandlePolicyV2Get()->Ops)xrtSetErrorKind(XERR_TYPE);
                /* The future and its unpublished payload have one private
                 * consumer. Transfer that credit; never copy/reparse the head. */
                else if(!xrtValueTakeHandle(value,&result)&&!xrtGetError())xrtSetErrorKind(XERR_STATE);
            }
        }
    }
done:
    {bool failed=xrtGetError()!=NULL;xl6_error_scope cleanup={0};
    if(!xl6ErrorScopeBeginCleanup(&cleanup,failed))abort();
    if(work)xl6TaskFrameDiscardInternalV1(work);
    xl6TlsIoSourceDropV1(future,failed);
    if(engine.resource)xl6ExecutionResourceLeaveV1(&engine);
    xl6ExecutionTlsLeaveV1(&stream);
    xl6_error_result error=xl6ErrorScopeEnd(&cleanup);
    if(!error.ok&&result){xl6Http1HeadDestroyV2(result);result=NULL;}
    xrtSetErrorTake(error.error);}
    return result;
}
void* xl6Http1HeadRequestTlsV2(void* receiver,int64_t maxHead,int64_t start,int64_t line,int64_t count,int64_t millis)
{xhttp1limits limits;return xl6HttpHeadLimits(&limits,maxHead,start,line,count)?perform(receiver,&limits,false,millis):NULL;}
void* xl6Http1HeadResponseTlsV2(void* receiver,int64_t maxHead,int64_t start,int64_t line,int64_t count,int64_t millis)
{xhttp1limits limits;return xl6HttpHeadLimits(&limits,maxHead,start,line,count)?perform(receiver,&limits,true,millis):NULL;}
