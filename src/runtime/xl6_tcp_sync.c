#include "xl6_tcp_internal.h"
#include "xl6_concurrent_deadline.h"
/* Explicit blocking methods reuse XRT's own Worker/deadline checks. Never
 * call the Worker-only xrtNetStreamRead on an arbitrary language thread. */
xvalue* xl6TcpStreamRecvV1(void* receiver,int64_t maximum,int64_t millis)
{
    size_t bytes;XL6_DeadlineV1 deadline;
    if(!xl6TcpPositiveSizeV1(maximum,&bytes)||!xl6ConcurrentDeadline(millis,&deadline)){xl6TcpFailedV1();return NULL;}
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return NULL;
    xnetbytes* native=xrtNetStreamRecv(scope.stream,bytes,xl6DeadlineRemainingMillisecondsV1(deadline),NULL);xvalue* result=NULL;
    if(native){result=xrtValueBytes(xrtNetBytesView(native));xrtNetBytesDestroy(native);}
    else if(xrtErrorKind(xrtGetError())==XERR_CLOSED){xrtClearError();result=xrtValueBytes((xbytesview){0});}
    xl6ExecutionTcpLeaveInternalV1(&scope);if(!result)xl6TcpFailedV1();return result;
}
int64_t xl6TcpStreamWaitV1(void* receiver,int64_t condition,int64_t millis)
{
    XL6_DeadlineV1 deadline;
    if(condition<XNET_STREAM_WAIT_OPEN||condition>XNET_STREAM_WAIT_CLOSE||!xl6ConcurrentDeadline(millis,&deadline)){
        if(condition<XNET_STREAM_WAIT_OPEN||condition>XNET_STREAM_WAIT_CLOSE)xrtSetErrorKind(XERR_RANGE);
        xl6TcpFailedV1();return XNET_RESULT_ERROR;
    }
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,false,&scope))return XNET_RESULT_ERROR;
    bool ok=xrtNetStreamWait(scope.stream,(xnetstreamwait)condition,xl6DeadlineRemainingMillisecondsV1(deadline),NULL);
    xl6ExecutionTcpLeaveInternalV1(&scope);if(ok)return XNET_RESULT_OK;
    xerrkind kind=xrtErrorKind(xrtGetError());
    if(kind==XERR_TIMEOUT||kind==XERR_CLOSED||kind==XERR_CANCELLED){
        xrtClearError();return kind==XERR_TIMEOUT?XNET_RESULT_TIMEOUT:kind==XERR_CLOSED?XNET_RESULT_CLOSED:XNET_RESULT_CANCELLED;
    }
    xl6TcpFailedV1();return XNET_RESULT_ERROR;
}
void* xl6TcpListenerAcceptV1(XL6_ExecutionServicesV1 services,void* receiver,int64_t millis)
{
    XL6_DeadlineV1 deadline;if(!xl6ConcurrentDeadline(millis,&deadline)){xl6TcpFailedV1();return NULL;}
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(receiver,XL6_EXECUTION_TCP_LISTENER_V1,true,&scope))return NULL;
    xnetstream* accepted=xrtNetListenerAcceptWait(scope.listener,xl6DeadlineRemainingMillisecondsV1(deadline),NULL);void* result=NULL;
    if(accepted){
        result=xl6ExecutionTcpAdoptV1(services.source,scope.engineOwner,accepted);
        if(!result){xerror* error=xrtTakeError();(void)xrtNetStreamAbort(accepted);xrtClearError();xrtSetErrorTake(error);}
        xrtNetStreamDestroy(accepted);
    }
    xl6ExecutionTcpLeaveInternalV1(&scope);if(!result)xl6TcpFailedV1();return result;
}
