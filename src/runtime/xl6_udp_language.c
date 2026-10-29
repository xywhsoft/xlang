#include "xl6_udp_internal.h"
static void* make(XL6_ExecutionServicesV1 services,void* engine,const void* local,const void* peer,const void* config,uint64_t affinity)
{
    void* result=xl6ExecutionUdpOpenV1(services.source,engine,local,peer,affinity,config);
    if(!result)xl6UdpFailedV1();
    return result;
}
void* xl6UdpBindV1(XL6_ExecutionServicesV1 services,void* engine,const void* local,const void* config,uint64_t affinity)
{return make(services,engine,local,NULL,config,affinity);}
void* xl6UdpBindDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* local)
{return make(services,engine,local,NULL,NULL,0);}
void* xl6UdpConnectV1(XL6_ExecutionServicesV1 services,void* engine,const void* peer,const void* config,uint64_t affinity)
{return make(services,engine,NULL,peer,config,affinity);}
void* xl6UdpConnectDefaultV1(XL6_ExecutionServicesV1 services,void* engine,const void* peer)
{return make(services,engine,NULL,peer,NULL,0);}
void* xl6UdpOpenV1(XL6_ExecutionServicesV1 services,void* engine,const void* local,const void* peer,const void* config,uint64_t affinity)
{return make(services,engine,local,peer,config,affinity);}
void xl6UdpDestroyV1(void* raw){xl6ExecutionResourceReleaseV1(raw);}
static void closeUdp(void* raw,bool abortUdp)
{
    XL6_UdpScopeV1 scope={0};if(!xl6UdpEnterV1(raw,false,&scope))return;
    bool ok=abortUdp?xl6ExecutionResourceCancelV1(raw):xl6ExecutionResourceCloseV1(raw);
    xl6ExecutionUdpLeaveInternalV1(&scope);if(!ok)xl6UdpFailedV1();
}
void xl6UdpCloseV1(void* raw){closeUdp(raw,false);}
void xl6UdpAbortV1(void* raw){closeUdp(raw,true);}
int64_t xl6UdpStateV1(void* raw)
{int state=0;if(!xl6ExecutionUdpStateV1(raw,&state))xl6UdpFailedV1();return state;}
bool xl6UdpConnectedV1(void* raw)
{bool result=false;if(!xl6ExecutionUdpConnectedV1(raw,&result))xl6UdpFailedV1();return result;}
int64_t xl6UdpSendControlAvailableV1(void* raw)
{uint32_t result=0;if(!xl6ExecutionUdpSendControlAvailableV1(raw,&result))xl6UdpFailedV1();return result;}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==xl6UdpHandlePolicyV1Get()->Ops&&
        visit(xl6ExecutionResourceOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6UdpHandlePolicyV1Get,xl6UdpDestroyV1,trace)
