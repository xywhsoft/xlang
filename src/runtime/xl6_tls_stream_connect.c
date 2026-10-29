#include "xl6_tls_transport_internal.h"
void* xl6TlsStreamCreateV1(XL6_ExecutionServicesV1 services,void* engine,const void* address,void* context,
    xstrview serverName,int64_t size,xstrview verifyName,int64_t verifySize,const void* stream,const void* tcp,int64_t worker)
{
    if(worker<0||(uint64_t)worker>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);xl6TlsTransportFailedV1();return NULL;}
    if(!xl6TlsTransportNameV1(&serverName,size)||!xl6TlsTransportNameV1(&verifyName,verifySize)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6ExecutionTlsConnectV1(services.source,engine,address,(uint32_t)worker,context,serverName,verifyName,stream,tcp);
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
void* xl6TlsStreamCreateDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* address,void* context,xstrview name,int64_t size)
{return xl6TlsStreamCreateV1(services,engine,address,context,name,size,(xstrview){0},-1,NULL,NULL,0);}
