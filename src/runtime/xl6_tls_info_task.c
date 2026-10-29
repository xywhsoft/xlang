#include "xl6_tls_metadata_internal.h"
#include "xl6_tls_io_internal.h"
#include "xl6_task_frame_internal.h"
#include "xl6_task_net.h"

typedef struct InfoWork {
    xvalue* receiver;
    xstrview field;
    const XL6_ClassIdentitySchemaV3* schema;
    bool classResult;
} InfoWork;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(InfoWork,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_STRING,offsetof(InfoWork,field),sizeof(xstrview)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(InfoWork,schema),sizeof(void*)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(InfoWork,classResult),sizeof(bool)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(InfoWork),4,slots};

static xvalue* object(InfoWork* work,XL6_TlsInfoV1* info)
{
    void* owned=info;
    if(!work->classResult)return xl6NativeHandleTakeV1(&owned,xl6TlsInfoHandlePolicyV1Get());
    XL6_NativeClassResultContextV1 context={
        {work->field.Data,work->field.Size,xl6TlsInfoHandlePolicyV1Get()},NULL,work->schema};
    xvalue* prepared=xl6NativeClassResultPrepareFromLeaseV1(context,xl6TaskFrameCodeLeaseInternalV1(work));
    if(!prepared){xl6TlsInfoDestroyV1(owned);return NULL;}
    return xl6NativeClassResultCommitTakeV1(context,&prepared,&owned);
}
static XL6_TaskOutcome run(xcancel* cancel,void* raw,XL6_TaskResult* output)
{
    InfoWork* work=raw;
    if(xrtCancelRequested(cancel))return XL6_TASK_CANCELLED;
    void *receiver=NULL,*user=NULL;const xvaluehandleops* ops=NULL;
    if(!xrtValueGetHandle(work->receiver,&receiver,&ops,&user)||!receiver||user||
        ops!=xl6TlsStreamHandlePolicyV1Get()->Ops){xrtSetErrorKind(XERR_TYPE);return XL6_TASK_FAILED;}
    XL6_TlsScopeV1 scope={0};
    if(!xl6ExecutionTlsStreamEnterV1(receiver,false,&scope))return XL6_TASK_FAILED;
    xvalue** box=xrtCalloc(1,sizeof(*box));
    *output=(XL6_TaskResult){box,box?xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE):NULL,NULL};
    if(box){
        XL6_TlsInfoV1* info=xl6TlsInfoBuildV1(scope.stream,scope.metadata);
        if(info)*box=object(work,info);
    }
    bool ok=box&&*box;
    if(!ok&&!xrtGetError())xrtSetErrorKind(XERR_STATE);
    xl6ExecutionTlsLeaveV1(&scope);
    return ok?XL6_TASK_SUCCESS:XL6_TASK_FAILED;
}
xfuture* xl6TlsInfoSourceV1(void* receiver,const XL6_NativeClassResultContextV1* context)
{
    if(context&&(context->field.policy!=xl6TlsInfoHandlePolicyV1Get()||
        !xl6NativeClassResultContextValidV1(*context))){
        if(!xrtGetError())xrtSetErrorKind(XERR_ARGUMENT);
        return NULL;
    }
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return NULL;
    XL6_TlsScopeV1 stream={0};XL6_ExecutionScopeV1 engine={0};
    InfoWork* work=NULL;xfuture* result=NULL;
    bool ok=xl6ExecutionTlsStreamEnterV1(receiver,false,&stream);
    xnetstream* transport=ok?xrtTlsStreamTransport(stream.stream):NULL;
    xnetworker* worker=transport?xrtNetStreamWorker(transport):NULL;
    if(ok&&!worker){xrtSetErrorKind(XERR_STATE);ok=false;}
    if(ok)ok=xl6ExecutionResourceEnterV1(stream.engineOwner,false,&engine);
    if(ok&&(!engine.engine||xrtNetWorkerEngine(worker)!=engine.engine)){xrtSetErrorKind(XERR_TYPE);ok=false;}
    if(ok)work=xl6TaskRuntimeV3Get()->Allocate(context?context->codeOwner:NULL,sizeof(*work),run,&layout);
    if(work){
        *work=(InfoWork){.schema=context?context->schema:NULL,.classResult=context!=NULL};
        void* owned=xl6ExecutionResourceRetainV1(receiver);
        if(owned)work->receiver=xl6NativeHandleTakeV1(&owned,xl6TlsStreamHandlePolicyV1Get());
        if(work->receiver&&context){
            char* name=xrtMalloc(context->field.fieldSize);
            if(name){memcpy(name,context->field.fieldName,context->field.fieldSize);work->field=(xstrview){name,context->field.fieldSize};}
        }
        if(work->receiver&&(!context||work->field.Data))
            result=xl6TaskNetSubmitOwnedPolicyV1(engine.engine,xrtNetWorkerIndex(worker),work,NULL,
                xl6PayloadPolicyGet(XL6_FUTURE_PAYLOAD_XVALUE),false,0);
    }
    if(!result){xl6ErrorScopeCaptureFailure(&errors);if(work)xl6TaskFrameDiscardInternalV1(work);}
    if(engine.resource)xl6ExecutionResourceLeaveV1(&engine);
    if(stream.execution.resource)xl6ExecutionTlsLeaveV1(&stream);
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){xrtFutureDestroy(result);xrtSetErrorTake(outcome.error);return NULL;}
    xrtErrorFree(outcome.error);return result;
}
xfuture* xl6TlsStreamSnapshotAsyncV1(XL6_NativeClassResultContextV1 context,void* receiver)
{
    xfuture* result=xl6TlsInfoSourceV1(receiver,&context);
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
