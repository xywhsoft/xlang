#include "xl6_tcp_server_internal.h"
void* xl6TcpServerPrepareAcceptV1(XL6_ExecutionServicesV1 services,void* receiver)
{
    void* result=xl6ExecutionTcpServerPrepareAcceptV1(services.source,receiver);
    if(!result)xl6TcpServerFailedV1();
    return result;
}
