#include "xl6_tcp_internal.h"
#include "xl6_concurrent_deadline.h"
static void* receive(void* receiver,int64_t maximum,uint64_t due,void* cancel,bool requireCancel)
{
    size_t bytes=0;
    if(!xl6TcpPositiveSizeV1(maximum,&bytes)||(requireCancel&&!cancel)){
        if(requireCancel&&!cancel)xrtSetErrorKind(XERR_ARGUMENT);
        xl6TcpFailedV1();return NULL;
    }
    XL6_TcpScopeV1 scope={0};if(!xl6TcpEnterV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return NULL;
    xnetbytes* result=xrtNetStreamRecv(scope.stream,bytes,xl6DeadlineRemainingMillisecondsV1(due),cancel);
    if(!result&&xrtErrorKind(xrtGetError())==XERR_CLOSED){xrtClearError();result=xrtNetBytesCopy(NULL,0);}
    xl6ExecutionTcpLeaveInternalV1(&scope);if(!result)xl6TcpFailedV1();return result;
}
void* xl6TcpStreamRecvOwnedV1(void* receiver,int64_t maximum,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TcpFailedV1();return NULL;}
    return receive(receiver,maximum,due,NULL,false);
}
void* xl6TcpStreamRecvOwnedCancelV1(void* receiver,void* cancel,int64_t maximum,int64_t millis)
{
    XL6_DeadlineV1 due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TcpFailedV1();return NULL;}
    return receive(receiver,maximum,due,cancel,true);
}
void* xl6TcpStreamRecvOwnedUntilV1(void* receiver,uint64_t due,int64_t maximum)
{return receive(receiver,maximum,due,NULL,false);}
void* xl6TcpStreamRecvOwnedUntilCancelV1(void* receiver,uint64_t due,void* cancel,int64_t maximum)
{return receive(receiver,maximum,due,cancel,true);}
