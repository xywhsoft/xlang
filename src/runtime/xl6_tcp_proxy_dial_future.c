#include "xl6_tcp_internal.h"
#include "xl6_execution_tcp_dial.h"
#include "xl6_net_resolver_input.h"
#include "xl6_tcp_future_internal.h"
xfuture* xl6TcpProxyDialStartV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,const void* proxy,xstrview input,int64_t port,const void* options)
{
    if(!proxy||port<=0||port>UINT16_MAX){xrtSetErrorKind(!proxy?XERR_ARGUMENT:XERR_RANGE);xl6TcpFailedV1();return NULL;}
    /* Transport family selects the PROXY endpoint. Do not constrain or resolve
     * the target locally; it is sent to the proxy as the validated exact host. */
    char* host=xl6NetResolverHostV1(input,XNET_FAMILY_UNSPEC);
    if(!host){xl6TcpFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=NULL;
    XL6_TcpClassCaptureV1* capture=xl6TcpClassCapturePrepareInternalV1(context,receiver,object,
        xl6TcpStreamHandlePolicyV1Get(),xl6ExecutionTcpPromoteDialV1,true,&carrier);
    if(!capture){xrtFree(host);return NULL;}
    xfuture* source=xl6ExecutionTcpProxyDialSourceV1(carrier,proxy,host,(uint16_t)port,options);
    xrtFree(host);
    return xl6TcpClassCapturePublishInternalV1(services,capture,source);
}
xfuture* xl6TcpProxyDialStartDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,const void* proxy,xstrview host,int64_t port)
{return xl6TcpProxyDialStartV1(services,context,receiver,object,proxy,host,port,NULL);}
