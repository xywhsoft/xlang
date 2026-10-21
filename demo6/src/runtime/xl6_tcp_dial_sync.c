#include "xl6_tcp_internal.h"
#include "xl6_execution_tcp_dial.h"
#include "xl6_execution_net_engine.h"
#include "xl6_net_resolver_input.h"
#include "xl6_future_payload.h"
/* Blocking is explicit and rejected on this Engine's own Worker BEFORE any
 * Dial is submitted. No language ready queue or per-wait thread is needed. */
static bool mayWait(void* engine)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(engine,true,&scope))return false;
    bool ok=((XL6_ExecutionResourceV1*)engine)->family==xl6ExecutionNetEngineFamilyV1Get();
    if(!ok)xrtSetErrorKind(XERR_TYPE);
    else if(xrtNetEngineCurrent(scope.engine)){xrtSetErrorKind(XERR_STATE);ok=false;}
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
static void* finish(XL6_ExecutionResourceV1* carrier,xfuture* source)
{
    bool ok=false;xfutureresult outcome={0};
    if(source&&xrtFutureWait(source)==XWAIT_OK&&xrtFutureResult(source,&outcome)&&
        xl6PayloadObserveOutcome(outcome.State,outcome.Error))ok=xl6ExecutionTcpPromoteDialV1(carrier);
    if(ok){xrtFutureDestroy(source);return carrier;}
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
    xrtFutureDestroy(source);xl6ExecutionResourceReleaseV1(carrier);
    xl6_error_result failure=xl6ErrorScopeEnd(&cleanup);xl6ErrorScopePropagateFailure(failure);return NULL;
}
void* xl6TcpDialV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,
    xstrview input,int64_t port,const void* options)
{
    if(!resolver||port<=0||port>UINT16_MAX){xrtSetErrorKind(!resolver?XERR_ARGUMENT:XERR_RANGE);xl6TcpFailedV1();return NULL;}
    const xnetdialconfig* config=options;
    char* host=xl6NetResolverHostV1(input,config?config->Family:XNET_FAMILY_UNSPEC);
    if(!host){xl6TcpFailedV1();return NULL;}
    if(!mayWait(engine)){xrtFree(host);xl6TcpFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=xl6ExecutionTcpPrepareDialV1(services.source,engine,resolver);
    xfuture* source=carrier?xl6ExecutionTcpDialSourceV1(carrier,host,(uint16_t)port,config):NULL;
    xrtFree(host);return finish(carrier,source);
}
void* xl6TcpDialDefaultV1(XL6_ExecutionServicesV1 services,void* engine,void* resolver,xstrview host,int64_t port)
{return xl6TcpDialV1(services,engine,resolver,host,port,NULL);}
void* xl6TcpDialAddressesV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses,const void* options)
{
    if(!addresses){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpFailedV1();return NULL;}
    if(!mayWait(engine)){xl6TcpFailedV1();return NULL;}
    XL6_ExecutionResourceV1* carrier=xl6ExecutionTcpPrepareDialV1(services.source,engine,NULL);
    xfuture* source=carrier?xl6ExecutionTcpDialAddressesSourceV1(carrier,addresses,options):NULL;
    return finish(carrier,source);
}
void* xl6TcpDialAddressesDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* addresses)
{return xl6TcpDialAddressesV1(services,engine,addresses,NULL);}
