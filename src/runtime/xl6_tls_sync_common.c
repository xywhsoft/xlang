#include "xl6_tls_io_internal.h"

bool xl6TlsIoPreWaitV1(uint64_t due,xcancel* parent)
{
    if(parent&&xrtCancelRequested(parent)){(void)xl6PayloadObserveOutcome(XFUTURE_CANCELLED,NULL);return false;}
    if(xl6DeadlineExpiredV1(due)){xrtSetErrorKind(XERR_TIMEOUT);return false;}
    return true;
}

bool xl6TlsIoMayWaitV1(XL6_ExecutionResourceV1* engine)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(engine,false,&scope))return false;
    bool ok=engine->family==xl6ExecutionNetEngineFamilyV1Get();
    if(!ok)xrtSetErrorKind(XERR_TYPE);
    else if(xrtNetEngineCurrent(scope.engine)){xrtSetErrorKind(XERR_STATE);ok=false;}
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6TlsIoWaitResultV1(xfuture* source,uint64_t due,xcancel* parent,xfutureresult* result)
{
    if(!source)return false;
    xwaitresult state=xrtFutureWaitForCancel(source,xl6DeadlineRemainingMillisecondsV1(due),parent);
    if(state==XWAIT_CANCELLED)(void)xl6PayloadObserveOutcome(XFUTURE_CANCELLED,NULL);
    else if(state==XWAIT_TIMEOUT)xrtSetErrorKind(XERR_TIMEOUT);
    else if(state==XWAIT_OK){
        if(parent&&xrtCancelRequested(parent))(void)xl6PayloadObserveOutcome(XFUTURE_CANCELLED,NULL);
        else if(xl6DeadlineExpiredV1(due))xrtSetErrorKind(XERR_TIMEOUT);
        else if(xrtFutureResult(source,result)){
            if(result->State==XFUTURE_CLOSED)return true;
            return xl6PayloadObserveOutcome(result->State,result->Error);
        }
    }
    return false;
}
void xl6TlsIoSourceDropV1(xfuture* source,bool stop)
{
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    /* Timeout is not a terminal event. XRT's certified activation owns its
     * producer/source/Stream until actual completion; dropping this creator
     * never drops borrowed language state. Do not wait indefinitely for a
     * Send which has already started and cannot be rolled back. */
    if(stop&&source&&xrtFutureState(source)==XFUTURE_PENDING)(void)xrtFutureCancel(source);
    xrtFutureDestroy(source);xl6_error_result result=xl6ErrorScopeEnd(&cleanup);xrtSetErrorTake(result.error);
}
