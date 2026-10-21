#include "xl6_tcp_server_internal.h"
#include "xl6_execution_tcp.h"
#include "xl6_concurrent_deadline.h"
static void* acceptUntil(XL6_ExecutionServicesV1 services,void* receiver,uint64_t due,void* cancel,bool requireCancel)
{
    if(requireCancel&&!cancel){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpServerFailedV1();return NULL;}
    XL6_TcpServerScopeV1 scope={0};if(!xl6TcpServerEnterV1(receiver,true,&scope))return NULL;
    xnetstream* accepted=xrtNetServerAcceptWait(scope.server,due,cancel);void* result=NULL;
    if(accepted){
        result=xl6ExecutionTcpAdoptV1(services.source,scope.engineOwner,accepted);
        if(!result){xerror* error=xrtTakeError();(void)xrtNetStreamAbort(accepted);xrtClearError();xrtSetErrorTake(error);}
        xrtNetStreamDestroy(accepted);
    }
    xl6ExecutionTcpServerLeaveInternalV1(&scope);if(!result)xl6TcpServerFailedV1();return result;
}
void* xl6TcpServerAcceptV1(XL6_ExecutionServicesV1 services,void* receiver,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TcpServerFailedV1();return NULL;}
    return acceptUntil(services,receiver,due,NULL,false);
}
void* xl6TcpServerAcceptCancelV1(XL6_ExecutionServicesV1 services,void* receiver,void* cancel,int64_t millis)
{
    xdeadline due;if(!xl6ConcurrentDeadline(millis,&due)){xl6TcpServerFailedV1();return NULL;}
    return acceptUntil(services,receiver,due,cancel,true);
}
void* xl6TcpServerAcceptUntilV1(XL6_ExecutionServicesV1 services,void* receiver,uint64_t due)
{return acceptUntil(services,receiver,due,NULL,false);}
void* xl6TcpServerAcceptUntilCancelV1(XL6_ExecutionServicesV1 services,void* receiver,uint64_t due,void* cancel)
{return acceptUntil(services,receiver,due,cancel,true);}
