#include "src/runtime/xl6_process_run_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

void xl6ProcessResultDestroyV1(void* handle)
{if(handle){xrtProcessResultUnit(handle);xrtFree(handle);}}
void* xl6ProcessRunV1(void* commandHandle,void* optionsHandle,const xvalue* input,int64_t size)
{
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(commandHandle);if(!command)return NULL;
    if(!optionsHandle){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xbytesview data;
    if(!input || !xrtValueGetBytes(input,&data))return NULL;
    xstrview selected={(const char*)data.Data,data.Size};if(!xl6TextSelectRaw(&selected,size))return NULL;
    XL6_ProcessRunOptions* source=optionsHandle;
    xprocessrunoptions options=source->options;options.Input=(xbytesview){(const unsigned char*)selected.Data,selected.Size};
    options.Timeout=source->timeoutMs>=0 ? source->timeoutMs :
        xl6DeadlineRemainingMillisecondsV1(source->deadline);
    xprocessresult* result=xrtCalloc(1,sizeof(*result));if(!result)return NULL;
    /* Infrastructure failure remains an exception, never a fabricated exit
     * code/result. Nonzero exit, timeout and cancellation are real results. */
    if(!xrtProcessRun(&command->config,&options,result)){xl6ProcessResultDestroyV1(result);return NULL;}
    return result;
}
static xprocessresult* resultCheck(void* handle)
{if(!handle)xrtSetErrorKind(XERR_ARGUMENT);return handle;}
#define RESULT(Name,Type,Expression) \
Type xl6ProcessResult##Name##V1(void* handle) {xprocessresult* r=resultCheck(handle);return r ? (Expression) : 0;}
RESULT(Success,bool,xrtProcessResultSuccess(r))
RESULT(TimedOut,bool,r->Wait==XWAIT_TIMEOUT)
RESULT(Cancelled,bool,r->Wait==XWAIT_CANCELLED)
RESULT(Wait,int64_t,r->Wait)
RESULT(InputWritten,uint64_t,r->InputWritten)
RESULT(Duration,uint64_t,r->Duration)
RESULT(StdoutTruncated,bool,r->StdoutTruncated)
RESULT(StderrTruncated,bool,r->StderrTruncated)
#undef RESULT
int64_t xl6ProcessResultExitCodeV1(void* handle)
{
    xprocessresult* r=resultCheck(handle);if(!r)return 0;
    if(r->Status.Kind!=XPROCESS_EXIT_CODE){xrtSetErrorKind(XERR_STATE);return 0;}
    return r->Status.Code;
}
xvalue* xl6ProcessResultStdoutV1(void* handle,int64_t size)
{
    xprocessresult* r=resultCheck(handle);if(!r)return NULL;
    xstrview data={(const char*)r->Stdout,r->StdoutSize};if(!xl6TextSelectRaw(&data,size))return NULL;
    return xrtValueBytes((xbytesview){(const unsigned char*)data.Data,data.Size});
}
xvalue* xl6ProcessResultStderrV1(void* handle,int64_t size)
{
    xprocessresult* r=resultCheck(handle);if(!r)return NULL;
    xstrview data={(const char*)r->Stderr,r->StderrSize};if(!xl6TextSelectRaw(&data,size))return NULL;
    return xrtValueBytes((xbytesview){(const unsigned char*)data.Data,data.Size});
}
xstrview xl6ProcessResultStdoutTextV1(void* handle,int64_t size)
{
    xprocessresult* r=resultCheck(handle);if(!r)return xl6TextFailure;
    xstrview data={(const char*)r->Stdout,r->StdoutSize};if(!xl6TextSelect(&data,size))return xl6TextFailure;
    return xl6TextCopy(data);
}
xstrview xl6ProcessResultStderrTextV1(void* handle,int64_t size)
{
    xprocessresult* r=resultCheck(handle);if(!r)return xl6TextFailure;
    xstrview data={(const char*)r->Stderr,r->StderrSize};if(!xl6TextSelect(&data,size))return xl6TextFailure;
    return xl6TextCopy(data);
}
void* xl6ProcessResultStatusV1(void* handle)
{
    xprocessresult* r=resultCheck(handle);if(!r)return NULL;
    xprocessstatus* snapshot=xrtMalloc(sizeof(*snapshot));if(snapshot)*snapshot=r->Status;return snapshot;
}

/* Resident policy: terminal Drop runs after the phased handle is detached. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ProcessResultHandlePolicyV1Get,xl6ProcessResultDestroyV1)
