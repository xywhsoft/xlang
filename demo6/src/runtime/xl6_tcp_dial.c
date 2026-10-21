#include "xl6_tcp_internal.h"
#include "xl6_execution_tcp_dial.h"
void* xl6TcpPrepareDialV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver)
{
    if(!resolver){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    void* result=xl6ExecutionTcpPrepareDialV1(services.source,engine,resolver);
    if(!result)xl6TcpFailedV1();
    return result;
}
void* xl6TcpPrepareDialAddressesV1(XL6_ExecutionServicesV1 services,void* engine)
{
    void* result=xl6ExecutionTcpPrepareDialV1(services.source,engine,NULL);
    if(!result)xl6TcpFailedV1();
    return result;
}
