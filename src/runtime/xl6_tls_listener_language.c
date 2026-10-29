#include "xl6_tls_transport_internal.h"
#include "xl6_net_address_abi.h"
void* xl6TlsListenerCreateV1(XL6_ExecutionServicesV1 services,void* engine,const void* listen,void* context,const void* stream,int64_t accepts,int64_t handshakes)
{
    if(!listen){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    if(accepts<=0||(uint64_t)accepts>UINT32_MAX||handshakes<=0||(uint64_t)handshakes>UINT32_MAX){
        xrtSetErrorKind(XERR_RANGE);xl6TlsTransportFailedV1();return NULL;}
    xtlslistenerconfig config;xrtTlsListenerConfigInit(&config);config.Listen=*(const xnetlistenconfig*)listen;
    if(stream)config.Stream=*(const xtlsstreamconfig*)stream;
    config.AcceptQueueLimit=(uint32_t)accepts;config.HandshakeLimit=(uint32_t)handshakes;
    void* result=xl6ExecutionTlsListenV1(services.source,engine,context,&config);
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
void* xl6TlsListenerCreateDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* listen,void* context)
{
    if(!listen){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();return NULL;}
    xtlslistenerconfig config;xrtTlsListenerConfigInit(&config);config.Listen=*(const xnetlistenconfig*)listen;
    void* result=xl6ExecutionTlsListenV1(services.source,engine,context,&config);
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
void xl6TlsListenerDestroyV1(void* raw){xl6ExecutionResourceReleaseV1(raw);}
void xl6TlsListenerCloseV1(void* raw)
{
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsListenerEnterV1(raw,false,&scope)){xl6TlsTransportFailedV1();return;}
    bool ok=xl6ExecutionResourceCloseV1(raw);xl6ExecutionTlsLeaveV1(&scope);if(!ok)xl6TlsTransportFailedV1();
}
int64_t xl6TlsListenerStateV1(void* raw)
{
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsListenerEnterV1(raw,false,&scope)){xl6TlsTransportFailedV1();return 0;}
    int64_t result=xrtTlsListenerState(scope.listener);xl6ExecutionTlsLeaveV1(&scope);return result;
}
void* xl6TlsListenerLocalV1(void* raw)
{
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsListenerEnterV1(raw,false,&scope)){xl6TlsTransportFailedV1();return NULL;}
    xnetaddr address;bool ok=xrtTlsListenerLocal(scope.listener,&address);xl6ExecutionTlsLeaveV1(&scope);
    if(!ok){xl6TlsTransportFailedV1();return NULL;}return xl6NetAddressCopyV1(&address);
}
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==xl6TlsListenerHandlePolicyV1Get()->Ops&&
        visit(xl6ExecutionResourceOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TlsListenerHandlePolicyV1Get,xl6TlsListenerDestroyV1,handleTrace)
