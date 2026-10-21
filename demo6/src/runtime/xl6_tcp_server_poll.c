#include "xl6_tcp_server_internal.h"
#include "xl6_execution_tcp.h"

void* xl6TcpServerTryAcceptV1(XL6_ExecutionServicesV1 services,void* receiver)
{
    XL6_TcpServerScopeV1 scope={0};
    if(!xl6TcpServerEnterV1(receiver,true,&scope))return NULL;
    xnetstream* accepted=xrtNetServerAccept(scope.server);
    void* result=NULL;
    bool failed=!accepted&&xrtGetError()!=NULL;
    if(accepted){
        result=xl6ExecutionTcpAdoptV1(services.source,scope.engineOwner,accepted);
        failed=!result;
        if(failed){
            /* Refused adoption must retire the dequeued owning native value,
             * preserving the original allocation/ownership diagnostic. */
            xerror* error=xrtTakeError();
            (void)xrtNetStreamAbort(accepted);
            xrtClearError();xrtSetErrorTake(error);
        }
        xrtNetStreamDestroy(accepted);
    }
    xl6ExecutionTcpServerLeaveInternalV1(&scope);
    if(failed)xl6TcpServerFailedV1();
    return result;
}
