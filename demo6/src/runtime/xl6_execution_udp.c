#include "xl6_execution_udp_internal.h"
#include "xl6_execution_net_engine.h"
#include <stdlib.h>
typedef XL6_ExecutionResourceV1 Resource;
typedef struct Udp {Resource base;Resource* parent;xatomicptr native;} Udp;
_Static_assert(offsetof(Udp,base)==0,"UDP resource base must be first");
static xnetudp* native(const Resource* r)
{return xrtAtomicPtrLoad(&((const Udp*)r)->native,XMEMORY_ACQUIRE);}
static void parentDrop(Udp* r)
{
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    Resource* parent=r->parent;r->parent=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6ExecutionResourceReleaseV1(parent);
}
static bool create(Resource* base,const XL6_ExecutionCreateOptionsV1* options)
{
    Udp* r=(Udp*)base;const XL6_UdpCreateOptionsV1* c=options->value.udp;xrtAtomicPtrInit(&r->native,NULL);
    if(!c||(!c->local&&!c->peer)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(c->engine,true,&scope))return false;
    if(c->engine->family!=xl6ExecutionNetEngineFamilyV1Get()||!scope.engine){
        xl6ExecutionResourceLeaveV1(&scope);xrtSetErrorKind(XERR_TYPE);return false;}
    r->parent=xl6ExecutionResourceRetainV1(c->engine);xnetudp* udp=NULL;
    if(r->parent){
        xnetudpconfig config;if(c->config)config=*c->config;else xrtNetUdpConfigInit(&config);
        udp=xrtNetUdpOpen(scope.engine,c->local,c->peer,c->affinity,&config,NULL,NULL);
    }
    xrtAtomicPtrStore(&r->native,udp,XMEMORY_RELEASE);xl6ExecutionResourceLeaveV1(&scope);return udp!=NULL;
}
static bool rollback(Resource* base)
{
    Udp* r=(Udp*)base;xnetudp* udp=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    /* Native callbacks never borrow this resident record. On publication
     * failure the real native runtime slot protects every dispatch tail. */
    if(udp){(void)xrtNetUdpAbort(udp);xrtNetUdpDestroy(udp);}parentDrop(r);return true;
}
static XL6_ModulePrepareResult retire(Resource* base)
{
    xnetudp* udp=native(base);if(!udp)return XL6_MODULE_PREPARE_READY;
    if(!xrtNetUdpAbort(udp))return XL6_MODULE_PREPARE_FAILED;
    if(xrtNetUdpState(udp)!=XNET_UDP_CLOSED)return XL6_MODULE_PREPARE_BUSY;
    return rollback(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
static bool finish(Resource* base)
{
    Udp* r=(Udp*)base;xnetudp* udp=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    /* All graph children are prepared AND cleared. Consume only the real
     * creator and parent; do not admit another Close through a cleared child. */
    xrtNetUdpDestroy(udp);parentDrop(r);return true;
}
static void drop(Resource* base){if(native(base)||((Udp*)base)->parent)abort();}
static bool trace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const Udp* r=(const Udp*)base;xnetudp* udp=native(base);
    return (!udp||visit(xrtNetUdpOwnership(udp),context))&&
        (!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context));
}
static bool closeUdp(Resource* r){xnetudp* udp=native(r);return !udp||xrtNetUdpClose(udp);}
static bool cancelUdp(Resource* r){xnetudp* udp=native(r);return !udp||xrtNetUdpAbort(udp);}
static bool present(const Resource* r){return native(r)!=NULL;}
const XL6_ExecutionFamilyV1* xl6ExecutionUdpFamilyV1Get(void)
{
    static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_UDP_SOCKET_V1,
        create,rollback,retire,finish,drop,trace,closeUdp,cancelUdp,sizeof(Udp),present};return &family;
}
Resource* xl6ExecutionUdpOpenV1(XL6_ExecutionSourceV1* source,Resource* engine,const xnetaddr* local,
    const xnetaddr* peer,uint64_t affinity,const xnetudpconfig* config)
{
    XL6_UdpCreateOptionsV1 udp={engine,local,peer,config,affinity};
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_UDP_SOCKET_V1,.value.udp=&udp};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionUdpFamilyV1Get(),&options);
}
Resource* xl6ExecutionUdpBindV1(XL6_ExecutionSourceV1* source,Resource* engine,const xnetaddr* address,
    uint64_t affinity,const xnetudpconfig* config){return xl6ExecutionUdpOpenV1(source,engine,address,NULL,affinity,config);}
bool xl6ExecutionUdpPostWorkV1(Resource* resource,XL6_Closure* callback)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(resource,true,&scope))return false;
    Udp* r=(Udp*)resource;xnetworker* worker=xrtNetUdpWorker(scope.udp);
    bool ok=worker&&r->parent&&xl6ExecutionNetEnginePostV1(r->parent,xrtNetWorkerIndex(worker),callback);
    if(!worker||!r->parent)xrtSetErrorKind(XERR_STATE);
    xl6ExecutionUdpLeaveInternalV1(&scope);return ok;
}
Resource* xl6ExecutionUdpConnectV1(XL6_ExecutionSourceV1* source,Resource* engine,const xnetaddr* address,
    uint64_t affinity,const xnetudpconfig* config){return xl6ExecutionUdpOpenV1(source,engine,NULL,address,affinity,config);}
bool xl6ExecutionUdpEnterInternalV1(Resource* r,bool submit,XL6_UdpScopeV1* scope)
{
    if(!scope||scope->execution.resource||scope->udp){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ExecutionResourceEnterV1(r,submit,&scope->execution))return false;
    if(r->kind!=XL6_EXECUTION_UDP_SOCKET_V1||r->family!=xl6ExecutionUdpFamilyV1Get()){
        xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_TYPE);return false;}
    scope->udp=native(r);if(scope->udp)return true;
    xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_STATE);return false;
}
void xl6ExecutionUdpLeaveInternalV1(XL6_UdpScopeV1* scope)
{xl6ExecutionResourceLeaveV1(&scope->execution);scope->udp=NULL;}
bool xl6ExecutionUdpStateV1(Resource* r,int* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}XL6_UdpScopeV1 scope={0};
    if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return false;
    *output=(int)xrtNetUdpState(scope.udp);xl6ExecutionUdpLeaveInternalV1(&scope);return true;
}
bool xl6ExecutionUdpLocalV1(Resource* r,xnetaddr* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}XL6_UdpScopeV1 scope={0};
    if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return false;
    bool ok=xrtNetUdpLocal(scope.udp,output);xl6ExecutionUdpLeaveInternalV1(&scope);return ok;
}
bool xl6ExecutionUdpPeerV1(Resource* r,xnetaddr* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}XL6_UdpScopeV1 scope={0};
    if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return false;
    bool ok=xrtNetUdpPeer(scope.udp,output);xl6ExecutionUdpLeaveInternalV1(&scope);return ok;
}
bool xl6ExecutionUdpConnectedV1(Resource* r,bool* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}XL6_UdpScopeV1 scope={0};
    if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return false;
    *output=xrtNetUdpConnected(scope.udp);xl6ExecutionUdpLeaveInternalV1(&scope);return true;
}
bool xl6ExecutionUdpSendControlAvailableV1(Resource* r,uint32_t* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}XL6_UdpScopeV1 scope={0};
    if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return false;
    *output=xrtNetUdpSendControlAvailable(scope.udp);xl6ExecutionUdpLeaveInternalV1(&scope);return true;
}
bool xl6ExecutionUdpStatsV1(Resource* r,xnetudpstats* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}XL6_UdpScopeV1 scope={0};
    if(!xl6ExecutionUdpEnterInternalV1(r,false,&scope))return false;
    bool ok=xrtNetUdpStats(scope.udp,output);xl6ExecutionUdpLeaveInternalV1(&scope);return ok;
}
