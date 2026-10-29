#include "xl6_execution_tcp_server_internal.h"
#include "xl6_execution_net_engine.h"
#include <stdlib.h>
typedef XL6_ExecutionResourceV1 Resource;
typedef struct Server {Resource base;Resource* parent;xatomicptr native;} Server;
_Static_assert(offsetof(Server,base)==0,"Typed resource base must be first");
static xnetserver* native(const Resource* r)
{return xrtAtomicPtrLoad(&((const Server*)r)->native,XMEMORY_ACQUIRE);}
static void parentDrop(Server* server)
{
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    Resource* parent=server->parent;server->parent=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6ExecutionResourceReleaseV1(parent);
}
static bool create(Resource* base,const XL6_ExecutionCreateOptionsV1* options)
{
    Server* r=(Server*)base;xrtAtomicPtrInit(&r->native,NULL);
    const XL6_TcpServerCreateOptionsV1* c=options->value.server;
    if(!c||!c->config){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(c->engine,true,&scope))return false;
    if(c->engine->family!=xl6ExecutionNetEngineFamilyV1Get()||!scope.engine){
        xl6ExecutionResourceLeaveV1(&scope);xrtSetErrorKind(XERR_TYPE);return false;}
    r->parent=xl6ExecutionResourceRetainV1(c->engine);
    xnetserver* value=r->parent?xrtNetServerStart(scope.engine,c->config,NULL,NULL,NULL):NULL;
    xrtAtomicPtrStore(&r->native,value,XMEMORY_RELEASE);xl6ExecutionResourceLeaveV1(&scope);return value!=NULL;
}
static bool rollback(Resource* base)
{
    Server* r=(Server*)base;xnetserver* value=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    if(value){(void)xrtNetServerClose(value);xrtNetServerDestroy(value);}
    /* Native runtime owns every callback tail and its actual Engine lease.
     * Dropping this creator does NOT claim those tails physically retired. */
    parentDrop(r);return true;
}
static XL6_ModulePrepareResult retire(Resource* base)
{
    xnetserver* value=native(base);if(!value)return XL6_MODULE_PREPARE_READY;
    if(!xrtNetServerClose(value))return XL6_MODULE_PREPARE_FAILED;
    if(xrtNetServerState(value)!=XNET_SERVER_CLOSED)return XL6_MODULE_PREPARE_BUSY;
    return rollback(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
static bool finish(Resource* base)
{
    Server* r=(Server*)base;xnetserver* value=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    /* Native child is independently prepared/cleared by the complete graph.
     * Never submit a new Close through that cleared native child. */
    xrtNetServerDestroy(value);parentDrop(r);return true;
}
static void drop(Resource* base)
{if(native(base)||((Server*)base)->parent)abort();}
static bool trace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const Server* r=(const Server*)base;xnetserver* value=native(base);
    return (!value||visit(xrtNetServerOwnership(value),context))&&
        (!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context));
}
static bool closeServer(Resource* base){xnetserver* value=native(base);return !value||xrtNetServerClose(value);}
static bool present(const Resource* base){return native(base)!=NULL;}
const XL6_ExecutionFamilyV1* xl6ExecutionTcpServerFamilyV1Get(void)
{
    static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,
        XL6_EXECUTION_TCP_SERVER_V1,create,rollback,retire,finish,drop,trace,closeServer,closeServer,sizeof(Server),present};return &family;
}
Resource* xl6ExecutionTcpServerCreateV1(XL6_ExecutionSourceV1* source,Resource* engine,const xnetserverconfig* config)
{
    XL6_TcpServerCreateOptionsV1 server={engine,config};
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TCP_SERVER_V1,.value.server=&server};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionTcpServerFamilyV1Get(),&options);
}
bool xl6ExecutionTcpServerEnterInternalV1(Resource* r,bool submit,XL6_TcpServerScopeV1* scope)
{
    if(!scope||scope->execution.resource){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ExecutionResourceEnterV1(r,submit,&scope->execution))return false;
    if(r->kind!=XL6_EXECUTION_TCP_SERVER_V1||r->family!=xl6ExecutionTcpServerFamilyV1Get()){
        xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_TYPE);return false;}
    scope->server=native(r);scope->engineOwner=((Server*)r)->parent;
    if(!scope->server){scope->engineOwner=NULL;xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
void xl6ExecutionTcpServerLeaveInternalV1(XL6_TcpServerScopeV1* scope)
{xl6ExecutionResourceLeaveV1(&scope->execution);scope->server=NULL;scope->engineOwner=NULL;}
