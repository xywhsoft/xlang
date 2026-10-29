#include "src/runtime/xl6_process_pipeline_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_process_run_internal.h"

void xl6PipelineResultDestroyV1(void* handle)
{
    if(!handle)return;
    xerror* prior=xrtTakeError();xrtProcessPipelineResultUnit(handle);xrtFree(handle);
    xrtClearError();xrtSetErrorTake(prior);
}
void* xl6ProcessPipelineRunV1(void* handle,void* optionsHandle,const xvalue* input,int64_t size)
{
    XL6_ProcessPipeline* pipeline=xl6ProcessPipelineCheck(handle);if(!pipeline)return NULL;
    if(!pipeline->count || !optionsHandle || !input){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xbytesview bytes;if(!xrtValueGetBytes(input,&bytes))return NULL;
    xstrview selected={(const char*)bytes.Data,bytes.Size};if(!xl6TextSelectRaw(&selected,size))return NULL;
    XL6_ProcessRunOptions* source=optionsHandle;
    xprocesspipelineoptions options;if(!xrtProcessPipelineOptionsInit(&options))return NULL;
    options.Input=(xbytesview){(const unsigned char*)selected.Data,selected.Size};
    options.Timeout=source->timeoutMs>=0 ? source->timeoutMs :
        xl6DeadlineRemainingMillisecondsV1(source->deadline);
    options.Cancel=source->options.Cancel;
    options.StopGrace=source->options.StopGrace;options.StdoutLimit=source->options.StdoutLimit;
    options.StderrLimit=source->options.StderrLimit;options.Overflow=source->options.Overflow;
    xprocesspipelineresult* result=xrtCalloc(1,sizeof(*result));if(!result)return NULL;
    if(!xrtProcessPipeline(pipeline->stages,pipeline->count,&options,result)){
        xl6PipelineResultDestroyV1(result);return NULL;
    }
    return result;
}
static xprocesspipelineresult* pipelineResultCheck(void* handle)
{if(!handle)xrtSetErrorKind(XERR_ARGUMENT);return handle;}
#define PIPELINE_RESULT(Name,Type,Expression) \
Type xl6PipelineResult##Name##V1(void* handle){xprocesspipelineresult* r=pipelineResultCheck(handle);return r ? (Expression) : 0;}
PIPELINE_RESULT(Success,bool,xrtProcessPipelineSuccess(r))
PIPELINE_RESULT(TimedOut,bool,r->Wait==XWAIT_TIMEOUT)
PIPELINE_RESULT(Cancelled,bool,r->Wait==XWAIT_CANCELLED)
PIPELINE_RESULT(Wait,int64_t,r->Wait)
PIPELINE_RESULT(Count,int64_t,(int64_t)r->StageCount)
PIPELINE_RESULT(InputWritten,uint64_t,r->InputWritten)
PIPELINE_RESULT(Duration,uint64_t,r->Duration)
PIPELINE_RESULT(StdoutTruncated,bool,r->StdoutTruncated)
#undef PIPELINE_RESULT
int64_t xl6PipelineResultCompletedV1(void* handle)
{
    xprocesspipelineresult* r=pipelineResultCheck(handle);if(!r)return 0;
    int64_t completed=0;
    for(size_t i=0;i<r->StageCount;++i)if(r->Stages[i].Status.Kind!=XPROCESS_EXIT_NONE)++completed;
    return completed;
}
int64_t xl6PipelineResultFailedIndexV1(void* handle)
{
    xprocesspipelineresult* r=pipelineResultCheck(handle);if(!r)return -1;
    for(size_t i=0;i<r->StageCount;++i){
        xprocessstatus* status=&r->Stages[i].Status;
        if(status->Kind!=XPROCESS_EXIT_CODE || status->Code!=0)return (int64_t)i;
    }
    return -1;
}
int64_t xl6PipelineResultExitCodeV1(void* handle)
{
    xprocesspipelineresult* r=pipelineResultCheck(handle);if(!r)return 0;
    if(!r->StageCount || r->Stages[r->StageCount-1].Status.Kind!=XPROCESS_EXIT_CODE){xrtSetErrorKind(XERR_STATE);return 0;}
    return r->Stages[r->StageCount-1].Status.Code;
}
xvalue* xl6PipelineResultStdoutV1(void* handle,int64_t size)
{
    xprocesspipelineresult* r=pipelineResultCheck(handle);if(!r)return NULL;
    xstrview data={(const char*)r->Stdout,r->StdoutSize};if(!xl6TextSelectRaw(&data,size))return NULL;
    return xrtValueBytes((xbytesview){(const unsigned char*)data.Data,data.Size});
}
xstrview xl6PipelineResultStdoutTextV1(void* handle,int64_t size)
{
    xprocesspipelineresult* r=pipelineResultCheck(handle);if(!r)return xl6TextFailure;
    xstrview data={(const char*)r->Stdout,r->StdoutSize};if(!xl6TextSelect(&data,size))return xl6TextFailure;
    return xl6TextCopy(data);
}
void xl6ProcessStageDestroyV1(void* handle)
{
    xprocessstageresult* stage=handle;if(!stage)return;
    xerror* prior=xrtTakeError();xrtFree(stage->Stderr);xrtFree(stage);
    xrtClearError();xrtSetErrorTake(prior);
}
void* xl6PipelineResultStageV1(void* handle,int64_t index)
{
    xprocesspipelineresult* r=pipelineResultCheck(handle);if(!r)return NULL;
    if(index<0 || (uint64_t)index>=r->StageCount){xrtSetErrorKind(XERR_RANGE);return NULL;}
    const xprocessstageresult* source=&r->Stages[index];
    xprocessstageresult* stage=xrtCalloc(1,sizeof(*stage));if(!stage)return NULL;
    stage->Status=source->Status;stage->StderrTruncated=source->StderrTruncated;
    if(source->StderrSize){
        stage->Stderr=xrtMalloc(source->StderrSize);
        if(!stage->Stderr){xl6ProcessStageDestroyV1(stage);return NULL;}
        memcpy(stage->Stderr,source->Stderr,source->StderrSize);stage->StderrSize=source->StderrSize;
    }
    return stage;
}
static xprocessstageresult* processStageCheck(void* handle)
{if(!handle)xrtSetErrorKind(XERR_ARGUMENT);return handle;}
bool xl6ProcessStageSuccessV1(void* handle)
{xprocessstageresult* s=processStageCheck(handle);return s && s->Status.Kind==XPROCESS_EXIT_CODE && s->Status.Code==0;}
bool xl6ProcessStageStderrTruncatedV1(void* handle)
{xprocessstageresult* s=processStageCheck(handle);return s && s->StderrTruncated;}
int64_t xl6ProcessStageExitCodeV1(void* handle)
{
    xprocessstageresult* s=processStageCheck(handle);if(!s)return 0;
    if(s->Status.Kind!=XPROCESS_EXIT_CODE){xrtSetErrorKind(XERR_STATE);return 0;}
    return s->Status.Code;
}
xvalue* xl6ProcessStageStderrV1(void* handle,int64_t size)
{
    xprocessstageresult* s=processStageCheck(handle);if(!s)return NULL;
    xstrview data={(const char*)s->Stderr,s->StderrSize};if(!xl6TextSelectRaw(&data,size))return NULL;
    return xrtValueBytes((xbytesview){(const unsigned char*)data.Data,data.Size});
}
xstrview xl6ProcessStageStderrTextV1(void* handle,int64_t size)
{
    xprocessstageresult* s=processStageCheck(handle);if(!s)return xl6TextFailure;
    xstrview data={(const char*)s->Stderr,s->StderrSize};if(!xl6TextSelect(&data,size))return xl6TextFailure;
    return xl6TextCopy(data);
}
void* xl6ProcessStageStatusV1(void* handle)
{
    xprocessstageresult* s=processStageCheck(handle);if(!s)return NULL;
    xprocessstatus* status=xrtMalloc(sizeof(*status));if(status)*status=s->Status;return status;
}

/* Resident policy: terminal Drop runs after the phased handle is detached. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6PipelineResultHandlePolicyV1Get,xl6PipelineResultDestroyV1)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ProcessStageHandlePolicyV1Get,xl6ProcessStageDestroyV1)
