/* Real accepted TaskPool jobs, not an outcome-notification mock. */
#define XRT_MODULE_TASK_POOL
#ifndef XL6_TASK_CHECKED_MINIMAL
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_closure.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

typedef struct Hooks {unsigned body,cleanup,runs,drops,resultDrops;} Hooks;
typedef struct Data {Hooks* hooks;XL6_Closure* closure;} Data;
typedef struct Environment {Hooks* hooks;} Environment;
typedef struct Balance {
#ifndef XL6_TASK_CHECKED_MINIMAL
    xmemdebugsnapshot snapshot;
#endif
    unsigned reserved;
} Balance;
static const unsigned environmentSchema=1;
static bool environmentValid(const void* schema,const void* data,size_t size)
{return schema==&environmentSchema&&data&&size==sizeof(Environment);}
static bool environmentTrace(const void* schema,const void* data,size_t size,XL6_ModuleOwnershipVisit visit,void* context)
{(void)visit;(void)context;return environmentValid(schema,data,size);}
static bool environmentDrop(const void* schema,void* data,size_t size)
{
    assert(environmentValid(schema,data,size));Environment* environment=data;Hooks* hooks=environment->hooks;
    ++hooks->drops;xrtFree(environment);
    if(hooks->cleanup==1){xrtSetErrorTake(xrtErrorCreate(XERR_STATE,"task.checked",902,"cleanup"));assert(xrtGetError());return false;}
    return hooks->cleanup!=2;
}
static const XL6_ClosureEnvironmentOpsV1 environmentOps={sizeof(environmentOps),environmentValid,environmentTrace,environmentDrop};
static bool callback(void* raw,int64_t* output)
{
    Environment* environment=raw;Hooks* hooks=environment->hooks;++hooks->runs;
    if(hooks->body==1){xrtSetErrorTake(xrtErrorCreate(XERR_STATE,"task.checked",901,"body"));assert(xrtGetError());return false;}
    if(hooks->body==2)return false;
    if(hooks->body==4){xrtSetErrorKind(XERR_CANCELLED);return false;}
    if(hooks->body==6)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if(hooks->body==7){
        xerror* error=xrtErrorCreate(XERR_STATE,"task.checked",901,"body checked");assert(error);
        xl6ErrorScopePropagateFailure((xl6_error_result){false,error});xrtClearError();
    }
    *output=321;return true;
}
static void invalidResultDrop(void* data,void* raw)
{Hooks* hooks=raw;++hooks->resultDrops;xrtFree(data);}
static XL6_TaskOutcome run(struct xcancel* cancel,void* raw,XL6_TaskResult* result)
{
    (void)cancel;Data* data=raw;
    if(data->hooks->body==3)return XL6_TASK_CANCELLED;
    int64_t* value=xrtCalloc(1,sizeof(*value));assert(value);
    *result=(XL6_TaskResult){value,xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_VALUE),NULL};
    XL6_ClosureScope entered={0};const XL6_ClosureRuntimeV1* closures=xl6ClosureRuntimeV1Get();
    closures->Enter(&entered,data->closure);
    bool ok=((bool (*)(void*,int64_t*))data->closure->invoke)(data->closure->environment,value);
    closures->Leave(&entered);
    if(data->hooks->body==5)*result=(XL6_TaskResult){value,invalidResultDrop,data->hooks};
    return ok?XL6_TASK_SUCCESS:XL6_TASK_FAILED;
}
static const XL6_CaptureSlotV2 slots[]={{{XL6_CAPTURE_CLOSURE,offsetof(Data,closure),sizeof(XL6_Closure*)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Data),1,slots};
static Data* create(Hooks* hooks)
{
    Data* data=xl6TaskRuntimeV3Get()->Allocate(NULL,sizeof(*data),run,&layout);assert(data);
    *data=(Data){hooks,NULL};Environment* environment=xrtMalloc(sizeof(*environment));assert(environment);environment->hooks=hooks;
    data->closure=xl6ClosureCreateEnvironmentV1(NULL,(void (*)(void))callback,NULL,environment,sizeof(*environment),
        &environmentSchema,&environmentOps,NULL,NULL,0);assert(data->closure);return data;
}
static void snapshot(Balance* output)
{
#ifndef XL6_TASK_CHECKED_MINIMAL
    xrtMemDebugSnapshot(&output->snapshot);
#else
    (void)output;
#endif
}
static void balanced(const Balance* before)
{
#ifndef XL6_TASK_CHECKED_MINIMAL
    const xmemdebugsnapshot* initial=&before->snapshot;
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==initial->LiveCount&&after.LiveBytes==initial->LiveBytes&&
        after.AllocCount-initial->AllocCount==after.FreeCount-initial->FreeCount&&
        after.InvalidFreeCount==initial->InvalidFreeCount&&after.DoubleFreeCount==initial->DoubleFreeCount&&
        after.UseAfterFreeCount==initial->UseAfterFreeCount);
#else
    (void)before;xrtClearError();
#endif
}
static void check(unsigned body,unsigned cleanup,bool preCancelled)
{
    Balance before={0};snapshot(&before);Hooks hooks={body,cleanup,0,0,0};
    const XL6_TaskRuntimeV1* tasks=xl6TaskRuntimeV1Get();XL6_TaskPoolConfig config={1,8,0};
    XL6_TaskPool* pool=tasks->PoolCreate(&config);assert(pool);Data* data=create(&hooks);
    xcancel* parent=preCancelled?xrtCancelCreate():NULL;
    if(preCancelled){assert(parent&&xrtCancelRequest(parent));}
    xfuture* future=xl6TaskSubmitPayloadV1(pool,data,parent,XL6_FUTURE_PAYLOAD_VALUE,true);assert(future);
    xrtCancelDestroy(parent);
    assert(tasks->PoolQuiesce(pool));xfutureresult result={0};assert(xrtFutureResult(future,&result));
    bool cancelled=preCancelled||body==3||body==4;
    bool failed=!cancelled&&(body==1||body==2||body==5||body==6||body==7||cleanup);
    xfuturestate expected=cancelled?XFUTURE_CANCELLED:failed?XFUTURE_FAILED:XFUTURE_RESOLVED;
    if(result.State!=expected)fprintf(stderr,"body=%u cleanup=%u preCancelled=%u: state=%d expected=%d\n",body,cleanup,preCancelled,(int)result.State,(int)expected);
    assert(result.State==expected);
    assert(hooks.drops==1&&hooks.runs==(preCancelled||body==3?0u:1u));
    if(!failed&&!cancelled)assert(result.Value&&*(int64_t*)result.Value==321);
    if(failed){
        assert(result.Error);
        if(body==1||body==7)assert(xrtErrorCode(result.Error)==901);
        else if(body==2||body==6){
            /* XRT's strict Reject contract requires a diagnostic. A missing
             * checked error materializes INTERNAL only at that boundary; a
             * later cleanup diagnostic cannot become the original body error. */
            assert(xrtErrorKind(result.Error)==XERR_INTERNAL);
        }else if(body==5)assert(xrtErrorKind(result.Error)==XERR_STATE&&xrtErrorCode(result.Error)!=902);
        else if(cleanup==2)assert(xrtErrorKind(result.Error)==XERR_INTERNAL);
        else assert(xrtErrorCode(result.Error)==902);
    }
    assert(hooks.resultDrops==(!preCancelled&&body==5?1u:0u));
    assert(tasks->PoolDestroy(pool));xrtFutureDestroy(future);balanced(&before);
}
int main(void)
{
#ifndef XL6_TASK_CHECKED_MINIMAL
    assert(xrtMemDebugEnable(true));
#endif
    xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    /* This first case is the regression: native body succeeds, capture Drop
     * fails, and the exposed language Future must not remain RESOLVED. */
    check(0,1,false);
    for(unsigned repeat=0;repeat<4;++repeat)
        for(unsigned body=0;body<8;++body)
            for(unsigned cleanup=0;cleanup<3;++cleanup)
                for(unsigned cancelled=0;cancelled<2;++cancelled)check(body,cleanup,cancelled!=0);
#ifdef XL6_TASK_CHECKED_MINIMAL
    puts("Task checked minimal: 193 accepted native jobs with exact planned sources/selectors; no test feature additions");
#else
    puts("Task checked outcome: 193 accepted native jobs; body/cleanup first failure, cancellation, strict native diagnostic boundary and exact balance");
#endif
    return 0;
}
