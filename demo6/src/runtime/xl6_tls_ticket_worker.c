#include "xl6_tls_ticket_worker.h"
/* Only complete owned inputs/immutable recipes cross the queue. A real Task
 * CodeLease protects schema storage; its borrowed CodeOwner is not retained.
 * No unfinished language result is allocated before the Worker runs. */
typedef struct Work {
    xvalue *receiver,*input;
    xstrview field;
    XL6_NativeClassResultContextV1 context;
    const XL6_TlsTicketCommandV1* command;
    uint32_t lifetime;
} Work;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_XVALUE,offsetof(Work,receiver),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_XVALUE,offsetof(Work,input),sizeof(xvalue*)},NULL},
    {{XL6_CAPTURE_STRING,offsetof(Work,field),sizeof(xstrview)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Work,context),sizeof(XL6_NativeClassResultContextV1)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Work,command),sizeof(void*)},NULL},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Work,lifetime),sizeof(uint32_t)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Work),6,slots};
static XL6_TaskOutcome run(xcancel* cancel,void* raw,XL6_TaskResult* output)
{
    Work* work=raw;if(xrtCancelRequested(cancel))return XL6_TASK_CANCELLED;
    void *receiver=NULL,*user=NULL;const xvaluehandleops* ops=NULL;
    if(!xrtValueGetHandle(work->receiver,&receiver,&ops,&user)||!receiver||user||
        ops!=xl6TlsStreamHandlePolicyV1Get()->Ops){xrtSetErrorKind(XERR_TYPE);return XL6_TASK_FAILED;}
    XL6_TlsScopeV1 scope={0};
    if(!xl6ExecutionTlsStreamEnterV1(receiver,work->command->mutates,&scope))return XL6_TASK_FAILED;
    xnetstream* tcp=xrtTlsStreamTransport(scope.stream);xnetworker* worker=tcp?xrtNetStreamWorker(tcp):NULL;
    xtlssession* session=xrtTlsStreamSession(scope.stream);xbytesview input={0};
    bool ok=worker&&xrtNetEngineCurrent(xrtNetWorkerEngine(worker))==worker&&session&&
        xrtTlsSessionRole(session)==work->command->role;
    if(!ok)xrtSetErrorKind(XERR_TYPE);
    if(ok&&work->input)ok=xrtValueGetBytes(work->input,&input);
    XL6_TaskOutcome result=ok?work->command->Run(scope.stream,work->context,
        xl6TaskFrameCodeLeaseInternalV1(work),cancel,input,work->lifetime,output):XL6_TASK_FAILED;
    if(result==XL6_TASK_FAILED&&!xrtGetError())xrtSetErrorKind(XERR_STATE);
    xl6ExecutionTlsLeaveV1(&scope);return result;
}
xfuture* xl6TlsTicketSubmitV1(const XL6_NativeClassResultContextV1* context,void* receiver,
    const XL6_TlsTicketCommandV1* command,xbytesview input,uint32_t lifetime)
{
    if(!command||!command->Run||(command->payload!=XL6_FUTURE_PAYLOAD_VALUE&&command->payload!=XL6_FUTURE_PAYLOAD_XVALUE)||
        (!!context!=(command->payload==XL6_FUTURE_PAYLOAD_XVALUE))){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return NULL;
    XL6_TlsScopeV1 stream={0};XL6_ExecutionScopeV1 engine={0};Work* work=NULL;xfuture* result=NULL;
    bool ok=xl6ExecutionTlsStreamEnterV1(receiver,command->mutates,&stream);
    xnetstream* tcp=ok?xrtTlsStreamTransport(stream.stream):NULL;
    xnetworker* worker=tcp?xrtNetStreamWorker(tcp):NULL;
    if(ok&&!worker){xrtSetErrorKind(XERR_STATE);ok=false;}
    if(ok)ok=xl6ExecutionResourceEnterV1(stream.engineOwner,false,&engine);
    if(ok&&(!engine.engine||xrtNetWorkerEngine(worker)!=engine.engine)){xrtSetErrorKind(XERR_TYPE);ok=false;}
    if(ok)work=xl6TaskRuntimeV3Get()->Allocate(context?context->codeOwner:NULL,sizeof(*work),run,&layout);
    if(work){
        *work=(Work){.command=command,.lifetime=lifetime};
        void* owned=xl6ExecutionResourceRetainV1(receiver);
        if(owned)work->receiver=xl6NativeHandleTakeV1(&owned,xl6TlsStreamHandlePolicyV1Get());
        ok=work->receiver!=NULL;
        if(ok&&context){
            char* field=xrtMalloc(context->field.fieldSize);ok=field!=NULL;
            if(ok){
                memcpy(field,context->field.fieldName,context->field.fieldSize);
                work->field=(xstrview){field,context->field.fieldSize};work->context=*context;
                work->context.field.fieldName=work->field.Data;work->context.codeOwner=NULL;
            }
        }
        /* Independent bytes snapshot, not a borrowed/COW assumption about a
         * language argument that its caller can modify after submission. */
        if(ok&&input.Size){work->input=xrtValueBytes(input);ok=work->input!=NULL;}
        if(ok)result=xl6TaskNetSubmitOwnedPolicyV1(engine.engine,xrtNetWorkerIndex(worker),work,NULL,
            xl6PayloadPolicyGet(command->payload),false,0);
    }
    if(!result){xl6ErrorScopeCaptureFailure(&errors);if(work)xl6TaskFrameDiscardInternalV1(work);}
    if(engine.resource)xl6ExecutionResourceLeaveV1(&engine);
    if(stream.execution.resource)xl6ExecutionTlsLeaveV1(&stream);
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){xrtFutureDestroy(result);xrtSetErrorTake(outcome.error);return NULL;}
    xrtErrorFree(outcome.error);return result;
}
