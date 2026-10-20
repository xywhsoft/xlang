#include "xl6_net_resolver_input.h"
#include "xl6_execution_net_resolver.h"
#include "xl6_native_handle.h"
void* xl6NetResolverCreateDefaultV1(XL6_ExecutionServicesV1 services)
{void* result=xl6ExecutionNetResolverCreateV1(services.source,NULL);if(!result)xl6NetResolverFailedV1();return result;}
void* xl6NetResolverCreateV1(XL6_ExecutionServicesV1 services,const void* config)
{
    if(!config){xrtSetErrorKind(XERR_ARGUMENT);xl6NetResolverFailedV1();return NULL;}
    void* result=xl6ExecutionNetResolverCreateV1(services.source,config);if(!result)xl6NetResolverFailedV1();return result;
}
void xl6NetResolverDestroyV1(void* raw){xl6ExecutionResourceReleaseV1(raw);}
void xl6NetResolverCloseV1(void* raw){if(!xl6ExecutionResourceCloseV1(raw))xl6NetResolverFailedV1();}
bool xl6NetResolverWaitV1(void* raw,int64_t millis)
{bool ready=false;if(!xl6ExecutionNetResolverWaitV1(raw,millis,&ready))xl6NetResolverFailedV1();return ready;}
void xl6NetResolverRetryV1(void* raw){if(!xl6ExecutionResourceRetryV1(raw))xl6NetResolverFailedV1();}
bool xl6NetResolverClosedV1(void* raw)
{
    XL6_ExecutionResourceSnapshotV1 value={.size=sizeof(value)};
    if(!xl6ExecutionResourceSnapshotV1Get(raw,&value))xl6NetResolverFailedV1();
    return value.closing;
}
void xl6NetResolverClearV1(void* raw){if(!xl6ExecutionNetResolverClearV1(raw))xl6NetResolverFailedV1();}
void* xl6NetResolverConfigurationV1(void* raw)
{
    xnetresolverconfig value;if(!xl6ExecutionNetResolverConfigV1(raw,&value)){xl6NetResolverFailedV1();return NULL;}
    xnetresolverconfig* result=xrtMalloc(sizeof(*result));if(result)*result=value;else xl6NetResolverFailedV1();return result;
}
void* xl6NetResolverSnapshotV1(void* raw)
{
    xnetresolverstats value;if(!xl6ExecutionNetResolverStatsV1(raw,&value)){xl6NetResolverFailedV1();return NULL;}
    xnetresolverstats* result=xrtMalloc(sizeof(*result));if(result)*result=value;else xl6NetResolverFailedV1();return result;
}
void* xl6NetResolverQueryV1(void* raw,xstrview input,int64_t family)
{
    char* host=xl6NetResolverHostV1(input,family);if(!host){xl6NetResolverFailedV1();return NULL;}
    xfuture* result=xl6ExecutionNetResolverQueryV1(raw,host,(int)family);xrtFree(host);
    if(!result)xl6NetResolverFailedV1();
    return result;
}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&
        ops==xl6NetResolverHandlePolicyV1Get()->Ops&&visit(xl6ExecutionResourceOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetResolverHandlePolicyV1Get,xl6NetResolverDestroyV1,trace)
