#include "src/runtime/xl6_process_internal.h"

void xl6ProcessRunOptionsDestroyV1(void* handle)
{
    XL6_ProcessRunOptions* options=handle;if(!options)return;
    xerror* prior=xrtTakeError();xrtCancelDestroy(options->options.Cancel);xrtFree(options);
    xrtClearError();xrtSetErrorTake(prior);
}
void* xl6ProcessRunOptionsCreateV1(int64_t timeoutMs,int64_t stdoutLimit,int64_t stderrLimit,
    int64_t overflow,int64_t stopGraceMs,uint64_t deadline)
{
    uint64 grace,ignored;
    if(timeoutMs < -1 || (timeoutMs>=0 && !xl6ProcessDuration(timeoutMs,&ignored)) ||
       !xl6ProcessDuration(stopGraceMs,&grace)) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    if(stdoutLimit<0 || stderrLimit<0 || (uint64_t)stdoutLimit>SIZE_MAX || (uint64_t)stderrLimit>SIZE_MAX ||
       overflow<XPROCESS_OVERFLOW_ERROR || overflow>XPROCESS_OVERFLOW_KEEP_LAST ||
       (deadline && timeoutMs!=-1)) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_ProcessRunOptions* options=xrtCalloc(1,sizeof(*options));if(!options)return NULL;
    if(!xrtProcessRunOptionsInit(&options->options))goto fail;
    options->options.Cancel=xrtCancelCreate();if(!options->options.Cancel)goto fail;
    options->timeoutMs=timeoutMs;options->options.Deadline=deadline ? deadline : XRT_DEADLINE_NEVER;
    options->options.StdoutLimit=(size_t)stdoutLimit;options->options.StderrLimit=(size_t)stderrLimit;
    options->options.Overflow=(xprocessoverflow)overflow;options->options.StopGrace=grace;
    return options;
fail:xl6ProcessRunOptionsDestroyV1(options);return NULL;
}
bool xl6ProcessRunOptionsCancelV1(void* handle)
{
    if(!handle){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return xrtCancelRequest(((XL6_ProcessRunOptions*)handle)->options.Cancel);
}
bool xl6ProcessRunOptionsRequestedV1(void* handle)
{
    if(!handle){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return xrtCancelRequested(((XL6_ProcessRunOptions*)handle)->options.Cancel);
}
