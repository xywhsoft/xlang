#include "xl6_tcp_internal.h"
#include "xl6_tcp_future_internal.h"
#include "xl6_execution_tcp_dial.h"
#include "xl6_net_resolver_input.h"
xfuture* xl6TcpDialStartV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,xstrview input,int64_t port,const void* options)
{
    if(port<=0||port>UINT16_MAX){xrtSetErrorKind(XERR_RANGE);xl6TcpFailedV1();return NULL;}
    const xnetdialconfig* config=options;
    char* host=xl6NetResolverHostV1(input,config?config->Family:XNET_FAMILY_UNSPEC);
    if(!host){xl6TcpFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=NULL;
    XL6_TcpClassCaptureV1* capture=xl6TcpClassCapturePrepareInternalV1(context,receiver,object,
        xl6TcpStreamHandlePolicyV1Get(),xl6ExecutionTcpPromoteDialV1,true,&carrier);
    if(!capture){xrtFree(host);return NULL;}
    xfuture* source=xl6ExecutionTcpDialSourceV1(carrier,host,(uint16_t)port,config);
    xrtFree(host); /* XRT copied the exact validated host during submission. */
    return xl6TcpClassCapturePublishInternalV1(services,capture,source);
}
xfuture* xl6TcpDialStartDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,xstrview host,int64_t port)
{return xl6TcpDialStartV1(services,context,receiver,object,host,port,NULL);}
xfuture* xl6TcpDialAddressesStartV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,const void* addresses,const void* options)
{
    if(!addresses){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=NULL;
    XL6_TcpClassCaptureV1* capture=xl6TcpClassCapturePrepareInternalV1(context,receiver,object,
        xl6TcpStreamHandlePolicyV1Get(),xl6ExecutionTcpPromoteDialV1,true,&carrier);
    if(!capture)return NULL;
    return xl6TcpClassCapturePublishInternalV1(services,capture,
        xl6ExecutionTcpDialAddressesSourceV1(carrier,addresses,options));
}
xfuture* xl6TcpDialAddressesStartDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,const void* addresses)
{return xl6TcpDialAddressesStartV1(services,context,receiver,object,addresses,NULL);}
