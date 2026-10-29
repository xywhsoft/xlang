#include "xl6_tcp_internal.h"
#include "xl6_execution_tcp_dial.h"
#include "xl6_net_resolver_input.h"
#include "xl6_tcp_dial_sync_internal.h"
#include "xl6_future_payload.h"
static void* dial(XL6_ExecutionServicesV1 services,void* engine,void* resolver,const void* proxy,
    xstrview input,int64_t port,const void* options,uint64_t due,void* parent,bool requireParent)
{
    if(!resolver||!proxy||(requireParent&&!parent)){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    if(port<=0||port>UINT16_MAX){xrtSetErrorKind(XERR_RANGE);xl6TcpFailedV1();return NULL;}
    char* host=xl6NetResolverHostV1(input,XNET_FAMILY_UNSPEC);
    if(!host){xl6TcpFailedV1();return NULL;}
    if(!xl6TcpDialMayWaitInternalV1(engine)){xrtFree(host);xl6TcpFailedV1();return NULL;}
    bool cancelled=parent&&xrtCancelRequested(parent);
    if(cancelled||xrtDeadlineExpired(due)){
        if(cancelled)(void)xl6PayloadObserveOutcome(XFUTURE_CANCELLED,NULL);else xrtSetErrorKind(XERR_TIMEOUT);
        xrtFree(host);xl6TcpFailedV1();return NULL;
    }
    XL6_ExecutionResourceV1* carrier=xl6ExecutionTcpPrepareDialV1(services.source,engine,resolver);
    xfuture* source=carrier?xl6ExecutionTcpProxyDialSourceV1(carrier,proxy,host,(uint16_t)port,options):NULL;
    xrtFree(host);return xl6TcpDialFinishUntilInternalV1(carrier,source,due,parent);
}
void* xl6TcpProxyDialDefaultV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port)
{return dial(services,engine,resolver,proxy,host,port,NULL,XRT_DEADLINE_NEVER,NULL,false);}
void* xl6TcpProxyDialV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,const void* options)
{return dial(services,engine,resolver,proxy,host,port,options,XRT_DEADLINE_NEVER,NULL,false);}
void* xl6TcpProxyDialCancelDefaultV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,void* parent)
{return dial(services,engine,resolver,proxy,host,port,NULL,XRT_DEADLINE_NEVER,parent,true);}
void* xl6TcpProxyDialCancelV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,const void* options,void* parent)
{return dial(services,engine,resolver,proxy,host,port,options,XRT_DEADLINE_NEVER,parent,true);}
void* xl6TcpProxyDialUntilDefaultV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,uint64_t due)
{return dial(services,engine,resolver,proxy,host,port,NULL,due,NULL,false);}
void* xl6TcpProxyDialUntilV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,const void* options,uint64_t due)
{return dial(services,engine,resolver,proxy,host,port,options,due,NULL,false);}
void* xl6TcpProxyDialUntilCancelDefaultV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,uint64_t due,void* parent)
{return dial(services,engine,resolver,proxy,host,port,NULL,due,parent,true);}
void* xl6TcpProxyDialUntilCancelV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,const void* proxy,xstrview host,int64_t port,const void* options,uint64_t due,void* parent)
{return dial(services,engine,resolver,proxy,host,port,options,due,parent,true);}
