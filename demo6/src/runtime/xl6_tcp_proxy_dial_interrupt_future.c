#include "xl6_tcp_internal.h"
#include "xl6_execution_tcp_dial.h"
#include "xl6_net_resolver_input.h"
#include "xl6_tcp_future_internal.h"
#include "xl6_future_callback_interrupt.h"
static xfuture* dial(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,const void* proxy,xstrview input,int64_t port,
    const void* options,uint64_t due,void* parent,bool requireParent)
{
    if(!services.executor||(due!=XRT_DEADLINE_NEVER&&!services.deadline)||!proxy||(requireParent&&!parent)){
        xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    if(port<=0||port>UINT16_MAX){xrtSetErrorKind(XERR_RANGE);xl6TcpFailedV1();return NULL;}
    char* host=xl6NetResolverHostV1(input,XNET_FAMILY_UNSPEC);
    if(!host){xl6TcpFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=NULL;
    XL6_TcpClassCaptureV1* capture=xl6TcpClassCapturePrepareInternalV1(context,receiver,object,
        xl6TcpStreamHandlePolicyV1Get(),xl6ExecutionTcpPromoteDialV1,true,&carrier);
    if(!capture){xrtFree(host);return NULL;}
    bool interrupted=parent||due!=XRT_DEADLINE_NEVER;
    if(interrupted&&!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due)){
        xrtFree(host);return xl6TcpClassCaptureFailInternalV1(capture,NULL);}
    bool stopped=interrupted&&xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:xl6ExecutionTcpProxyDialSourceV1(carrier,proxy,host,(uint16_t)port,options);
    xrtFree(host);
    if(!source&&!stopped)return xl6TcpClassCaptureFailInternalV1(capture,NULL);
    if(!interrupted)return xl6TcpClassCapturePublishInternalV1(services,capture,source);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TcpClassCaptureFailInternalV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TcpProxyDialStartCancelDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* proxy,xstrview host,int64_t port,void* parent)
{return dial(services,context,receiver,object,proxy,host,port,NULL,XRT_DEADLINE_NEVER,parent,true);}
xfuture* xl6TcpProxyDialStartCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* proxy,xstrview host,int64_t port,const void* options,void* parent)
{return dial(services,context,receiver,object,proxy,host,port,options,XRT_DEADLINE_NEVER,parent,true);}
xfuture* xl6TcpProxyDialStartUntilDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* proxy,xstrview host,int64_t port,uint64_t due)
{return dial(services,context,receiver,object,proxy,host,port,NULL,due,NULL,false);}
xfuture* xl6TcpProxyDialStartUntilV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* proxy,xstrview host,int64_t port,const void* options,uint64_t due)
{return dial(services,context,receiver,object,proxy,host,port,options,due,NULL,false);}
xfuture* xl6TcpProxyDialStartUntilCancelDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* proxy,xstrview host,int64_t port,uint64_t due,void* parent)
{return dial(services,context,receiver,object,proxy,host,port,NULL,due,parent,true);}
xfuture* xl6TcpProxyDialStartUntilCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* proxy,xstrview host,int64_t port,const void* options,uint64_t due,void* parent)
{return dial(services,context,receiver,object,proxy,host,port,options,due,parent,true);}
