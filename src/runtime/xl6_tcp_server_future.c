#include "xl6_tcp_server_internal.h"
#include "xl6_tcp_future_internal.h"
xfuture* xl6TcpServerAcceptStartV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object)
{
    XL6_ExecutionResourceV1* carrier=NULL;
    XL6_TcpClassCaptureV1* capture=xl6TcpClassCapturePrepareInternalV1(context,receiver,object,
        xl6TcpServerHandlePolicyV1Get(),xl6ExecutionTcpServerPromoteAcceptV1,false,&carrier);
    if(!capture)return NULL;
    return xl6TcpClassCapturePublishInternalV1(services,capture,xl6ExecutionTcpServerAcceptSourceForV1(carrier,receiver));
}
