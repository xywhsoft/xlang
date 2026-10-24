#include "xl6_execution_tls_internal.h"
#include <stdlib.h>
typedef XL6_ExecutionResourceV1 Resource;
typedef struct Stream {Resource base;Resource* parent;xatomicptr native;} Stream;
_Static_assert(offsetof(Stream,base)==0,"TLS resource base must be first");
static xtlsstream* native(const Resource* r)
{return xrtAtomicPtrLoad(&((const Stream*)r)->native,XMEMORY_ACQUIRE);}
const xtlsstreamownershipv1* xl6ExecutionTlsStreamDataPolicyV1Get(void)
{
    static const xtlsstreamownershipv1 policy={sizeof(policy),{0},xl6ExecutionResourceNativeDropV1,&xl6ExecutionResourceNativeOpsV1};
    return &policy;
}
static bool create(Resource* base,const XL6_ExecutionCreateOptionsV1* options)
{
    Stream* r=(Stream*)base;const XL6_TlsStreamCreateOptionsV1* c=options->value.tlsStream;
    xrtAtomicPtrInit(&r->native,NULL);
    if(!c||((c->accepted!=NULL)==(c->Build!=NULL))){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionTlsEngineEnterV1(c->engine,&scope))return false;
    r->parent=xl6ExecutionResourceRetainV1(c->engine);xtlsstream* result=NULL;
    if(r->parent){
        if(c->accepted){
            xnetstream* tcp=xrtTlsStreamTransport(c->accepted);
            xnetworker* worker=tcp?xrtNetStreamWorker(tcp):NULL;
            if(!worker||xrtNetWorkerEngine(worker)!=scope.engine)xrtSetErrorKind(XERR_ARGUMENT);
            else result=xrtTlsStreamRef(c->accepted);
        }else if(xl6ExecutionResourceNativePinV1(base)){
            result=c->Build(scope.engine,base,xl6ExecutionTlsStreamDataPolicyV1Get(),c);
            if(!result)xl6ExecutionResourceNativeDropV1(base);
        }
    }
    xrtAtomicPtrStore(&r->native,result,XMEMORY_RELEASE);xl6ExecutionResourceLeaveV1(&scope);
    return result!=NULL;
}
static bool dispose(Resource* base)
{
    Stream* r=(Stream*)base;xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))abort();
    xtlsstream* stream=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    if(!xrtOwnershipScopeEnd(&m))abort();
    xrtTlsStreamDestroy(stream);xl6ExecutionTlsParentDropV1(&r->parent);return true;
}
static bool rollback(Resource* base)
{
    /* Native Data owns a real Resource pin. Never free an unpublished record
     * while accepted startup/callback tails may still drop that pin. */
    return native(base)?false:dispose(base);
}
static XL6_ModulePrepareResult retire(Resource* base)
{
    xtlsstream* stream=native(base);if(!stream)return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
    if(!xrtTlsStreamAbort(stream))return XL6_MODULE_PREPARE_FAILED;
    xtlsstreamstate state=xrtTlsStreamState(stream);xnetstream* tcp=xrtTlsStreamTransport(stream);
    if((state!=XTLS_STREAM_CLOSED&&state!=XTLS_STREAM_FAILED)||(tcp&&xrtNetStreamState(tcp)!=XNET_STREAM_CLOSED))return XL6_MODULE_PREPARE_BUSY;
    return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
static void drop(Resource* base)
{if(native(base)||((Stream*)base)->parent)abort();}
static bool trace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const Stream* r=(const Stream*)base;xtlsstream* stream=native(base);
    return (!stream||visit(xrtTlsStreamOwnership(stream),context))&&
        (!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context));
}
static bool closeStream(Resource* base){xtlsstream* s=native(base);return !s||xrtTlsStreamClose(s);}
static bool cancelStream(Resource* base){xtlsstream* s=native(base);return !s||xrtTlsStreamAbort(s);}
static bool present(const Resource* base){return native(base)!=NULL;}
const XL6_ExecutionFamilyV1* xl6ExecutionTlsStreamFamilyV1Get(void)
{
    static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_TLS_STREAM_V1,
        create,rollback,retire,dispose,drop,trace,closeStream,cancelStream,sizeof(Stream),present};return &family;
}
Resource* xl6ExecutionTlsAdoptV1(XL6_ExecutionSourceV1* source,Resource* engine,xtlsstream* accepted)
{
    if(!accepted){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    const XL6_TlsStreamCreateOptionsV1 tls={.engine=engine,.accepted=accepted};
    const XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TLS_STREAM_V1,.deferRollback=true,.value.tlsStream=&tls};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionTlsStreamFamilyV1Get(),&options);
}
bool xl6ExecutionTlsStreamEnterV1(Resource* base,bool submit,XL6_TlsScopeV1* scope)
{
    if(!scope||scope->execution.resource){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ExecutionResourceEnterV1(base,submit,&scope->execution))return false;
    if(base->family!=xl6ExecutionTlsStreamFamilyV1Get()||base->kind!=XL6_EXECUTION_TLS_STREAM_V1){
        xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_TYPE);return false;}
    scope->stream=native(base);scope->engineOwner=((Stream*)base)->parent;
    if(!scope->stream){scope->engineOwner=NULL;xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
