#include "xl6_tcp_server_internal.h"
#include "xl6_tcp_server_options_internal.h"
void* xl6TcpServerCreateV1(XL6_ExecutionServicesV1 services,void* engine,const void* options)
{
    if(!options){xrtSetErrorKind(XERR_ARGUMENT);xl6TcpServerFailedV1();return NULL;}
    void* result=xl6ExecutionTcpServerCreateV1(services.source,engine,xl6TcpServerOptionsNativeV1(options));
    if(!result)xl6TcpServerFailedV1();
    return result;
}
void xl6TcpServerDestroyV1(void* raw){xl6ExecutionResourceReleaseV1(raw);}
void xl6TcpServerCloseV1(void* raw)
{
    XL6_TcpServerScopeV1 scope={0};if(!xl6TcpServerEnterV1(raw,false,&scope))return;
    bool ok=xl6ExecutionResourceCloseV1(raw);xl6ExecutionTcpServerLeaveInternalV1(&scope);if(!ok)xl6TcpServerFailedV1();
}
int64_t xl6TcpServerStateV1(void* raw)
{
    XL6_TcpServerScopeV1 scope={0};if(!xl6TcpServerEnterV1(raw,false,&scope))return 0;
    int64_t result=xrtNetServerState(scope.server);xl6ExecutionTcpServerLeaveInternalV1(&scope);return result;
}
uint64_t xl6TcpServerEndpointCountV1(void* raw)
{
    XL6_TcpServerScopeV1 scope={0};if(!xl6TcpServerEnterV1(raw,false,&scope))return 0;
    uint64_t result=xrtNetServerEndpointCount(scope.server);xl6ExecutionTcpServerLeaveInternalV1(&scope);return result;
}
uint64_t xl6TcpServerListenerCountV1(void* raw)
{
    XL6_TcpServerScopeV1 scope={0};if(!xl6TcpServerEnterV1(raw,false,&scope))return 0;
    uint64_t result=xrtNetServerListenerCount(scope.server);xl6ExecutionTcpServerLeaveInternalV1(&scope);return result;
}
void* xl6TcpServerLocalV1(void* raw,int64_t index)
{
    if(index<0||(uint64_t)index>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);xl6TcpServerFailedV1();return NULL;}
    XL6_TcpServerScopeV1 scope={0};if(!xl6TcpServerEnterV1(raw,false,&scope))return NULL;
    xnetaddr address;bool ok=xrtNetServerLocal(scope.server,(size_t)index,&address);
    xl6ExecutionTcpServerLeaveInternalV1(&scope);if(!ok){xl6TcpServerFailedV1();return NULL;}
    void* result=xl6NetAddressCopyV1(&address);if(!result)xl6TcpServerFailedV1();return result;
}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==xl6TcpServerHandlePolicyV1Get()->Ops&&
        visit(xl6ExecutionResourceOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TcpServerHandlePolicyV1Get,xl6TcpServerDestroyV1,trace)
