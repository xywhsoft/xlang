#include "xl6_tls_transport_internal.h"
#include "xl6_net_address_abi.h"
void* xl6TlsStreamCreateV1(XL6_ExecutionServicesV1 services,void* engine,const void* address,void* context,
    xstrview serverName,int64_t size,xstrview verifyName,int64_t verifySize,const void* stream,const void* tcp,int64_t worker)
{
    if(worker<0||(uint64_t)worker>UINT32_MAX){xrtSetErrorKind(XERR_RANGE);xl6TlsTransportFailedV1();return NULL;}
    if(!xl6TlsTransportNameV1(&serverName,size)||!xl6TlsTransportNameV1(&verifyName,verifySize)){xl6TlsTransportFailedV1();return NULL;}
    void* result=xl6ExecutionTlsConnectV1(services.source,engine,address,(uint32_t)worker,context,serverName,verifyName,stream,tcp);
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
void* xl6TlsStreamCreateDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* address,void* context,xstrview name,int64_t size)
{return xl6TlsStreamCreateV1(services,engine,address,context,name,size,(xstrview){0},-1,NULL,NULL,0);}
void xl6TlsStreamDestroyV1(void* raw){xl6ExecutionResourceReleaseV1(raw);}
static void request(void* raw,bool cancel)
{
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsStreamEnterV1(raw,false,&scope)){xl6TlsTransportFailedV1();return;}
    bool ok=cancel?xl6ExecutionResourceCancelV1(raw):xl6ExecutionResourceCloseV1(raw);
    xl6ExecutionTlsLeaveV1(&scope);if(!ok)xl6TlsTransportFailedV1();
}
void xl6TlsStreamCloseV1(void* raw){request(raw,false);}
void xl6TlsStreamAbortV1(void* raw){request(raw,true);}
int64_t xl6TlsStreamStateV1(void* raw)
{
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsStreamEnterV1(raw,false,&scope)){xl6TlsTransportFailedV1();return 0;}
    int64_t result=xrtTlsStreamState(scope.stream);xl6ExecutionTlsLeaveV1(&scope);return result;
}
static uint64_t size(void* raw,bool pending)
{
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsStreamEnterV1(raw,false,&scope)){xl6TlsTransportFailedV1();return 0;}
    uint64_t result=pending?xrtTlsStreamPending(scope.stream):xrtTlsStreamAvailable(scope.stream);xl6ExecutionTlsLeaveV1(&scope);return result;
}
uint64_t xl6TlsStreamAvailableV1(void* raw){return size(raw,false);}
uint64_t xl6TlsStreamPendingV1(void* raw){return size(raw,true);}
static void* address(void* raw,bool remote)
{
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsStreamEnterV1(raw,false,&scope)){xl6TlsTransportFailedV1();return NULL;}
    xnetstream* tcp=xrtTlsStreamTransport(scope.stream);xnetaddr result;
    bool ok=tcp&&(remote?xrtNetStreamRemote(tcp,&result):xrtNetStreamLocal(tcp,&result));
    xl6ExecutionTlsLeaveV1(&scope);if(!ok){if(!xrtGetError())xrtSetErrorKind(XERR_STATE);xl6TlsTransportFailedV1();return NULL;}
    return xl6NetAddressCopyV1(&result);
}
void* xl6TlsStreamLocalV1(void* raw){return address(raw,false);}
void* xl6TlsStreamRemoteV1(void* raw){return address(raw,true);}
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==xl6TlsStreamHandlePolicyV1Get()->Ops&&
        visit(xl6ExecutionResourceOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6TlsStreamHandlePolicyV1Get,xl6TlsStreamDestroyV1,handleTrace)
