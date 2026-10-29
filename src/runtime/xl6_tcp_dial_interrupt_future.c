#include "xl6_tcp_internal.h"
#include "xl6_tcp_future_internal.h"
#include "xl6_execution_tcp_dial.h"
#include "xl6_net_resolver_input.h"
#include "xl6_future_callback_interrupt.h"

/* Real class/capture preparation precedes arming; real operation submission
 * follows it. No onCancel language closure or second Promise is registered
 * after starting I/O. All twelve declarations share this single transport. */
static xfuture* dial(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,
    void* receiver,const xvalue* object,xstrview input,int64_t port,const void* addresses,
    const void* options,uint64_t due,void* parent,bool byAddresses,bool requireParent)
{
    if(!services.executor||(due!=XL6_DEADLINE_NEVER&&!services.deadline)||
        (requireParent&&!parent)||(byAddresses&&!addresses)){
        xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    if(!byAddresses&&(port<=0||port>UINT16_MAX)){xrtSetErrorKind(XERR_RANGE);xl6TcpFailedV1();return NULL;}
    const xnetdialconfig* config=options;char* host=NULL;
    if(!byAddresses){host=xl6NetResolverHostV1(input,config?config->Family:XNET_FAMILY_UNSPEC);
        if(!host){xl6TcpFailedV1();return NULL;}}
    XL6_ExecutionResourceV1* carrier=NULL;
    XL6_TcpClassCaptureV1* capture=xl6TcpClassCapturePrepareInternalV1(context,receiver,object,
        xl6TcpStreamHandlePolicyV1Get(),xl6ExecutionTcpPromoteDialV1,true,&carrier);
    if(!capture){xrtFree(host);return NULL;}
    bool interrupted=parent||due!=XL6_DEADLINE_NEVER;
    if(interrupted&&!xl6FutureCallbackInterruptArmV1(capture,parent,services.deadline,due)){
        xrtFree(host);return xl6TcpClassCaptureFailInternalV1(capture,NULL);}
    bool stopped=interrupted&&xl6FutureCallbackInterruptStoppedV1(capture);
    xfuture* source=stopped?NULL:byAddresses?xl6ExecutionTcpDialAddressesSourceV1(carrier,addresses,config):
        xl6ExecutionTcpDialSourceV1(carrier,host,(uint16_t)port,config);
    xrtFree(host);
    if(!source&&!stopped)return xl6TcpClassCaptureFailInternalV1(capture,NULL);
    if(!interrupted)return xl6TcpClassCapturePublishInternalV1(services,capture,source);
    xfuture* result=xl6FutureCallbackInterruptStartV1(services.executor,capture,source);
    if(!result)return xl6TcpClassCaptureFailInternalV1(capture,source);
    xrtFutureDestroy(source);return result;
}
xfuture* xl6TcpDialStartCancelDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,xstrview host,int64_t port,void* parent)
{return dial(services,context,receiver,object,host,port,NULL,NULL,XL6_DEADLINE_NEVER,parent,false,true);}
xfuture* xl6TcpDialStartCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,xstrview host,int64_t port,const void* options,void* parent)
{return dial(services,context,receiver,object,host,port,NULL,options,XL6_DEADLINE_NEVER,parent,false,true);}
xfuture* xl6TcpDialStartUntilDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,xstrview host,int64_t port,uint64_t due)
{return dial(services,context,receiver,object,host,port,NULL,NULL,due,NULL,false,false);}
xfuture* xl6TcpDialStartUntilV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,xstrview host,int64_t port,const void* options,uint64_t due)
{return dial(services,context,receiver,object,host,port,NULL,options,due,NULL,false,false);}
xfuture* xl6TcpDialStartUntilCancelDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,xstrview host,int64_t port,uint64_t due,void* parent)
{return dial(services,context,receiver,object,host,port,NULL,NULL,due,parent,false,true);}
xfuture* xl6TcpDialStartUntilCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,xstrview host,int64_t port,const void* options,uint64_t due,void* parent)
{return dial(services,context,receiver,object,host,port,NULL,options,due,parent,false,true);}
xfuture* xl6TcpDialAddressesStartCancelDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* addresses,void* parent)
{return dial(services,context,receiver,object,(xstrview){0},0,addresses,NULL,XL6_DEADLINE_NEVER,parent,true,true);}
xfuture* xl6TcpDialAddressesStartCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* addresses,const void* options,void* parent)
{return dial(services,context,receiver,object,(xstrview){0},0,addresses,options,XL6_DEADLINE_NEVER,parent,true,true);}
xfuture* xl6TcpDialAddressesStartUntilDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* addresses,uint64_t due)
{return dial(services,context,receiver,object,(xstrview){0},0,addresses,NULL,due,NULL,true,false);}
xfuture* xl6TcpDialAddressesStartUntilV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* addresses,const void* options,uint64_t due)
{return dial(services,context,receiver,object,(xstrview){0},0,addresses,options,due,NULL,true,false);}
xfuture* xl6TcpDialAddressesStartUntilCancelDefaultV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* addresses,uint64_t due,void* parent)
{return dial(services,context,receiver,object,(xstrview){0},0,addresses,NULL,due,parent,true,true);}
xfuture* xl6TcpDialAddressesStartUntilCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* receiver,const xvalue* object,const void* addresses,const void* options,uint64_t due,void* parent)
{return dial(services,context,receiver,object,(xstrview){0},0,addresses,options,due,parent,true,true);}
