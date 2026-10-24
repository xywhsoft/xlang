#include "xl6_execution_tls_internal.h"
#include <stdlib.h>
typedef XL6_ExecutionResourceV1 Resource;
typedef XL6_TlsStreamResourceV1 Stream;
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
    xrtAtomicPtrInit(&r->pending,NULL);xrtAtomic32Init(&r->prepared,0);
    if(!c||((c->accepted!=NULL)+(c->Build!=NULL)+(c->accepting!=NULL)+c->dialing!=1)||
        (c->resolving&&!c->dialing)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionTlsEngineEnterV1(c->engine,&scope))return false;
    r->parent=xl6ExecutionResourceRetainV1(c->engine);xtlsstream* result=NULL;
    if(r->parent){
        if(c->dialing){
            r->pendingKind=XL6_TLS_PENDING_DIAL_V1;
            if(c->resolving)r->resolving=xl6ExecutionResourceRetainV1(c->resolving);
            if(!c->resolving||r->resolving)xrtAtomic32Store(&r->prepared,1,XMEMORY_RELEASE);
        }else if(c->accepting){
            r->pendingKind=XL6_TLS_PENDING_ACCEPT_V1;
            r->accepting=xl6ExecutionResourceRetainV1(c->accepting);
            if(r->accepting)xrtAtomic32Store(&r->prepared,1,XMEMORY_RELEASE);
        }else if(c->accepted){
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
    return result!=NULL||xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE)!=0;
}
static bool dispose(Resource* base)
{
    Stream* r=(Stream*)base;xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))abort();
    xtlsstream* stream=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    xfuture* pending=xrtAtomicPtrExchange(&r->pending,NULL,XMEMORY_ACQ_REL);
    xrtAtomic32Store(&r->prepared,0,XMEMORY_RELEASE);
    if(!xrtOwnershipScopeEnd(&m))abort();
    xrtFutureDestroy(pending);xrtTlsStreamDestroy(stream);
    xl6ExecutionTlsParentDropV1(&r->accepting);xl6ExecutionTlsParentDropV1(&r->resolving);
    xl6ExecutionTlsParentDropV1(&r->parent);return true;
}
static bool rollback(Resource* base)
{
    /* Native Data owns a real Resource pin. Never free an unpublished record
     * while accepted startup/callback tails may still drop that pin. */
    return native(base)?false:dispose(base);
}
static XL6_ModulePrepareResult retire(Resource* base)
{
    Stream* r=(Stream*)base;xtlsstream* stream=native(base);
    if(!stream){
        xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
        if(pending&&xrtFutureState(pending)==XFUTURE_PENDING){
            (void)xrtFutureCancel(pending);
            if(xrtFutureState(pending)==XFUTURE_PENDING)return XL6_MODULE_PREPARE_BUSY;
        }
        /* A stop is not proof of native cancellation. A resolved, undelivered
         * Stream remains owned by this real Future until physical close. */
        if(pending&&xrtFutureState(pending)==XFUTURE_RESOLVED){
            xtlsstream* undelivered=xrtFutureValue(pending);
            if(undelivered){
                if(!xrtTlsStreamAbort(undelivered))return XL6_MODULE_PREPARE_FAILED;
                xtlsstreamstate state=xrtTlsStreamState(undelivered);
                xnetstream* tcp=xrtTlsStreamTransport(undelivered);
                if((state!=XTLS_STREAM_CLOSED&&state!=XTLS_STREAM_FAILED)||
                    (tcp&&xrtNetStreamState(tcp)!=XNET_STREAM_CLOSED))return XL6_MODULE_PREPARE_BUSY;
            }
        }
        return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
    }
    if(!xrtTlsStreamAbort(stream))return XL6_MODULE_PREPARE_FAILED;
    xtlsstreamstate state=xrtTlsStreamState(stream);xnetstream* tcp=xrtTlsStreamTransport(stream);
    if((state!=XTLS_STREAM_CLOSED&&state!=XTLS_STREAM_FAILED)||(tcp&&xrtNetStreamState(tcp)!=XNET_STREAM_CLOSED))return XL6_MODULE_PREPARE_BUSY;
    return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
static void drop(Resource* base)
{const Stream* r=(const Stream*)base;if(native(base)||r->parent||r->accepting||r->resolving||
    xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE)||xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE))abort();}
static bool trace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const Stream* r=(const Stream*)base;xtlsstream* stream=native(base);
    xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
    return (!stream||visit(xrtTlsStreamOwnership(stream),context))&&
        (!pending||visit(xrtFutureOwnership(pending),context))&&
        (!r->accepting||visit(xl6ExecutionResourceOwnershipV1(r->accepting),context))&&
        (!r->resolving||visit(xl6ExecutionResourceOwnershipV1(r->resolving),context))&&
        (!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context));
}
static bool closeStream(Resource* base)
{
    xtlsstream* s=native(base);if(s)return xrtTlsStreamClose(s);
    xfuture* pending=xrtAtomicPtrLoad(&((Stream*)base)->pending,XMEMORY_ACQUIRE);
    if(pending&&xrtFutureState(pending)==XFUTURE_PENDING)(void)xrtFutureCancel(pending);
    return true;
}
static bool cancelStream(Resource* base){xtlsstream* s=native(base);return s?xrtTlsStreamAbort(s):closeStream(base);}
static bool present(const Resource* base)
{const Stream* r=(const Stream*)base;return native(base)!=NULL||
    xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE)!=NULL||xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE)!=0;}
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
