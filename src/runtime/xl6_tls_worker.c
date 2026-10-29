#include "xl6_tls_data_internal.h"
#include "xl6_task_frame_internal.h"
#include "xl6_task_net.h"

/* A real shared Task frame is owned by TaskNet's certified activation. Its
 * only graph child is the actual receiver handle; the source/promise/job/
 * engine remain the existing task topology. No stack command or wait thread. */
typedef struct Work {xvalue* receiver;int64_t argument;unsigned operation;} Work;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(Work,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Work,argument),sizeof(int64_t)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Work,operation),sizeof(unsigned)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Work),3,slots};
static bool isBytes(unsigned operation)
{return operation==XL6_TLS_WORKER_PEEK_V1||operation==XL6_TLS_WORKER_PROTOCOL_V1;}
static xvalue* peek(xtlsstream* stream,int64_t maximum)
{
    const xnetbuf* buffer=xrtTlsStreamBuffer(stream);if(!buffer)return NULL;
    size_t count=maximum<0||(uint64_t)maximum>buffer->Size?buffer->Size:(size_t)maximum;
    if(count>INT64_MAX){xrtSetErrorKind(XERR_RANGE);return NULL;}
    if(!count)return xrtValueBytes((xbytesview){0});
    uint8* data=xrtMalloc(count);if(!data)return NULL;
    xvalue* result=NULL;
    if(xrtNetBufPeek(buffer,0,data,count)!=count)xrtSetErrorKind(XERR_STATE);
    else result=xrtValueBytesTake(&data,count);
    xrtFree(data);return result;
}
static XL6_TaskOutcome run(xcancel* cancel,void* raw,XL6_TaskResult* output)
{
    Work* work=raw;if(xrtCancelRequested(cancel))return XL6_TASK_CANCELLED;
    void *receiver=NULL,*user=NULL;const xvaluehandleops* ops=NULL;
    if(!xrtValueGetHandle(work->receiver,&receiver,&ops,&user)||!receiver||user||ops!=xl6TlsStreamHandlePolicyV1Get()->Ops){
        xrtSetErrorKind(XERR_TYPE);return XL6_TASK_FAILED;}
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsStreamEnterV1(receiver,xl6TlsWorkerMutatesV1(work->operation),&scope))return XL6_TASK_FAILED;
    xnetstream* tcp=xrtTlsStreamTransport(scope.stream);xnetworker* worker=tcp?xrtNetStreamWorker(tcp):NULL;
    bool ok=worker&&xrtNetEngineCurrent(xrtNetWorkerEngine(worker))==worker;
    if(!ok)xrtSetErrorKind(XERR_STATE);
    XL6_FuturePayloadKind kind=isBytes(work->operation)?XL6_FUTURE_PAYLOAD_XVALUE:XL6_FUTURE_PAYLOAD_VALUE;
    void* box=ok?xrtCalloc(1,isBytes(work->operation)?sizeof(xvalue*):sizeof(int64_t)):NULL;
    if(ok&&!box)ok=false;
    *output=(XL6_TaskResult){box,box?xl6PayloadDropGet(kind):NULL,NULL};
    if(ok){
        switch(work->operation){
        case XL6_TLS_WORKER_PEEK_V1:
            *(xvalue**)box=peek(scope.stream,work->argument);ok=*(xvalue**)box!=NULL;break;
        case XL6_TLS_WORKER_PROTOCOL_V1:{
            xbytesview protocol={0};xtlssession* session=xrtTlsStreamSession(scope.stream);
            ok=session!=NULL;
            if(ok&&!xrtTlsSessionProtocol(session,&protocol)&&xrtGetError())ok=false;
            if(ok){*(xvalue**)box=xrtValueBytes(protocol);ok=*(xvalue**)box!=NULL;}
            break;
        }
        case XL6_TLS_WORKER_CONSUME_V1:
            ok=xrtTlsStreamConsume(scope.stream,(size_t)work->argument);
            if(ok)*(int64_t*)box=work->argument;
            break;
        case XL6_TLS_WORKER_SEND_BOUND_V1:{
            size_t count=0;ok=xrtTlsStreamSendBound(scope.stream,(size_t)work->argument,&count);
            if(ok&&count>INT64_MAX){xrtSetErrorKind(XERR_RANGE);ok=false;}
            if(ok)*(int64_t*)box=(int64_t)count;
            break;
        }
        case XL6_TLS_WORKER_KEY_UPDATE_V1:{
            xtlssession* session=xrtTlsStreamSession(scope.stream);
            if(!session||!scope.metadata||!scope.metadata->KeyUpdate||
                xrtTlsSessionRole(session)!=scope.metadata->role){
                xrtSetErrorKind(XERR_TYPE);ok=false;break;
            }
            xtlsresult status=scope.metadata->KeyUpdate(session,work->argument?
                XTLS_KEY_UPDATE_REQUESTED:XTLS_KEY_UPDATE_NOT_REQUESTED);
            if(status==XTLS_OK){
                xtlsresult flushed=xrtTlsStreamFlush(scope.stream);
                /* Once produced, a control record is accepted even when TCP
                 * is backpressured: its normal driver owns the retry. Never
                 * invite callers to repeat an already committed key update. */
                if(flushed!=XTLS_OK&&flushed!=XTLS_AGAIN)status=flushed;
            }
            *(int64_t*)box=status;ok=status!=XTLS_ERROR&&!xrtGetError();break;
        }
        default:xrtSetErrorKind(XERR_ARGUMENT);ok=false;break;
        }
    }
    if(!ok&&!xrtGetError())xrtSetErrorKind(XERR_STATE);
    xl6ExecutionTlsLeaveV1(&scope);return ok?XL6_TASK_SUCCESS:XL6_TASK_FAILED;
}
xfuture* xl6TlsWorkerSourceV1(void* receiver,int64_t argument,unsigned operation)
{
    if(operation>XL6_TLS_WORKER_KEY_UPDATE_V1||argument<(operation==XL6_TLS_WORKER_PEEK_V1?-1:0)||
        (operation==XL6_TLS_WORKER_KEY_UPDATE_V1&&argument>1)||
        (argument>=0&&(uint64_t)argument>SIZE_MAX)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return NULL;
    XL6_TlsScopeV1 stream={0};XL6_ExecutionScopeV1 engine={0};Work* work=NULL;xfuture* result=NULL;
    bool ok=xl6ExecutionTlsStreamEnterV1(receiver,xl6TlsWorkerMutatesV1(operation),&stream);
    xnetstream* tcp=ok?xrtTlsStreamTransport(stream.stream):NULL;
    xnetworker* worker=tcp?xrtNetStreamWorker(tcp):NULL;
    if(ok&&!worker){xrtSetErrorKind(XERR_STATE);ok=false;}
    /* This is work on an already admitted child, not a new user Engine job.
     * Keep its real Engine alive even after logical Close; native Stop still
     * rejects publication. Arbitrary public Engine submissions remain closed. */
    if(ok)ok=xl6ExecutionResourceEnterV1(stream.engineOwner,false,&engine);
    if(ok&&(!engine.engine||xrtNetWorkerEngine(worker)!=engine.engine)){xrtSetErrorKind(XERR_TYPE);ok=false;}
    if(ok)work=xl6TaskRuntimeV3Get()->Allocate(NULL,sizeof(*work),run,&layout);
    if(work){
        *work=(Work){.argument=argument,.operation=operation};void* owned=xl6ExecutionResourceRetainV1(receiver);
        if(owned)work->receiver=xl6NativeHandleTakeV1(&owned,xl6TlsStreamHandlePolicyV1Get());
        if(work->receiver)result=xl6TaskNetSubmitOwnedPolicyV1(engine.engine,xrtNetWorkerIndex(worker),work,NULL,
            xl6PayloadPolicyGet(isBytes(operation)?XL6_FUTURE_PAYLOAD_XVALUE:XL6_FUTURE_PAYLOAD_VALUE),false,0);
    }
    if(!result){xl6ErrorScopeCaptureFailure(&errors);if(work)xl6TaskFrameDiscardInternalV1(work);}
    if(engine.resource)xl6ExecutionResourceLeaveV1(&engine);
    if(stream.execution.resource)xl6ExecutionTlsLeaveV1(&stream);
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){xrtFutureDestroy(result);xrtSetErrorTake(outcome.error);return NULL;}
    xrtErrorFree(outcome.error);return result;
}
