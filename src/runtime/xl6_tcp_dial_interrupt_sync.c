#include "xl6_tcp_internal.h"
#include "xl6_execution_tcp_dial.h"
#include "xl6_tcp_dial_sync_internal.h"
#include "xl6_net_resolver_input.h"
#include "xl6_future_payload.h"
static void* dial(XL6_ExecutionServicesV1 services,void* engine,void* resolver,
    xstrview input,int64_t port,const void* addresses,const void* options,uint64_t due,
    void* parent,bool direct,bool requireParent)
{
    if((direct?!addresses:!resolver)||(requireParent&&!parent)){
        xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    if(!direct&&(port<=0||port>UINT16_MAX)){xrtSetErrorKind(XERR_RANGE);xl6TcpFailedV1();return NULL;}
    const xnetdialconfig* config=options;char* host=NULL;
    if(!direct){host=xl6NetResolverHostV1(input,config?config->Family:XNET_FAMILY_UNSPEC);
        if(!host){xl6TcpFailedV1();return NULL;}}
    if(!xl6TcpDialMayWaitInternalV1(engine)){xrtFree(host);xl6TcpFailedV1();return NULL;}
    bool cancelled=parent&&xrtCancelRequested(parent);
    if(cancelled||xrtDeadlineExpired(due)){
        if(cancelled)(void)xl6PayloadObserveOutcome(XFUTURE_CANCELLED,NULL);
        else xrtSetErrorKind(XERR_TIMEOUT);
        xrtFree(host);xl6TcpFailedV1();return NULL;
    }
    XL6_ExecutionResourceV1* carrier=xl6ExecutionTcpPrepareDialV1(services.source,engine,direct?NULL:resolver);
    xfuture* source=!carrier?NULL:direct?xl6ExecutionTcpDialAddressesSourceV1(carrier,addresses,config):
        xl6ExecutionTcpDialSourceV1(carrier,host,(uint16_t)port,config);
    xrtFree(host);return xl6TcpDialFinishUntilInternalV1(carrier,source,due,parent);
}
void* xl6TcpDialCancelDefaultV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,xstrview host,int64_t port,void* parent)
{return dial(services,engine,resolver,host,port,NULL,NULL,XRT_DEADLINE_NEVER,parent,false,true);}
void* xl6TcpDialCancelV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,xstrview host,int64_t port,const void* options,void* parent)
{return dial(services,engine,resolver,host,port,NULL,options,XRT_DEADLINE_NEVER,parent,false,true);}
void* xl6TcpDialAddressesCancelDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses,void* parent)
{return dial(services,engine,NULL,(xstrview){0},0,addresses,NULL,XRT_DEADLINE_NEVER,parent,true,true);}
void* xl6TcpDialAddressesCancelV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses,const void* options,void* parent)
{return dial(services,engine,NULL,(xstrview){0},0,addresses,options,XRT_DEADLINE_NEVER,parent,true,true);}
void* xl6TcpDialUntilDefaultV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,xstrview host,int64_t port,uint64_t due)
{return dial(services,engine,resolver,host,port,NULL,NULL,due,NULL,false,false);}
void* xl6TcpDialUntilV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,xstrview host,int64_t port,const void* options,uint64_t due)
{return dial(services,engine,resolver,host,port,NULL,options,due,NULL,false,false);}
void* xl6TcpDialAddressesUntilDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses,uint64_t due)
{return dial(services,engine,NULL,(xstrview){0},0,addresses,NULL,due,NULL,true,false);}
void* xl6TcpDialAddressesUntilV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses,const void* options,uint64_t due)
{return dial(services,engine,NULL,(xstrview){0},0,addresses,options,due,NULL,true,false);}
void* xl6TcpDialUntilCancelDefaultV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,xstrview host,int64_t port,uint64_t due,void* parent)
{return dial(services,engine,resolver,host,port,NULL,NULL,due,parent,false,true);}
void* xl6TcpDialUntilCancelV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,xstrview host,int64_t port,const void* options,uint64_t due,void* parent)
{return dial(services,engine,resolver,host,port,NULL,options,due,parent,false,true);}
void* xl6TcpDialAddressesUntilCancelDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses,uint64_t due,void* parent)
{return dial(services,engine,NULL,(xstrview){0},0,addresses,NULL,due,parent,true,true);}
void* xl6TcpDialAddressesUntilCancelV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses,const void* options,uint64_t due,void* parent)
{return dial(services,engine,NULL,(xstrview){0},0,addresses,options,due,parent,true,true);}
