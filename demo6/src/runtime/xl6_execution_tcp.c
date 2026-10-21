#include "xl6_execution_tcp_internal.h"
#include "xl6_execution_net_engine.h"
#include <stdlib.h>
typedef XL6_ExecutionResourceV1 Resource;
typedef struct Stream {Resource base;Resource *parent,*accepting;xatomicptr native,pending;xatomic32 prepared;} Stream;
typedef struct Listener {Resource base;Resource* parent;xatomicptr native;} Listener;
_Static_assert(offsetof(Stream,base)==0&&offsetof(Listener,base)==0,"Typed resource base must be first");
static xnetstream* stream(const Resource* r)
{return xrtAtomicPtrLoad(&((const Stream*)r)->native,XMEMORY_ACQUIRE);}
static xnetlistener* listener(const Resource* r)
{return xrtAtomicPtrLoad(&((const Listener*)r)->native,XMEMORY_ACQUIRE);}
static bool engineEnter(Resource* engine,XL6_ExecutionScopeV1* scope)
{
    if(!xl6ExecutionResourceEnterV1(engine,true,scope))return false;
    if(engine->family!=xl6ExecutionNetEngineFamilyV1Get()||!scope->engine){
        xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_TYPE);return false;}
    return true;
}
static void parentDrop(Resource** slot)
{
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    Resource* parent=*slot;*slot=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6ExecutionResourceReleaseV1(parent);
}
static bool streamCreate(Resource* base,const XL6_ExecutionCreateOptionsV1* options)
{
    Stream* r=(Stream*)base;const XL6_TcpCreateOptionsV1* c=options->value.tcp;
    xrtAtomicPtrInit(&r->native,NULL);
    xrtAtomicPtrInit(&r->pending,NULL);
    xrtAtomic32Init(&r->prepared,0);
    if(!c||(!c->accepted&&!c->accepting&&!c->remote)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!engineEnter(c->engine,&scope))return false;
    r->parent=xl6ExecutionResourceRetainV1(c->engine);xnetstream* native=NULL;
    if(r->parent){
        if(c->accepting){
            r->accepting=xl6ExecutionResourceRetainV1(c->accepting);
            if(r->accepting)xrtAtomic32Store(&r->prepared,1,XMEMORY_RELEASE);
        }else if(c->accepted){
            xnetworker* worker=xrtNetStreamWorker(c->accepted);
            if(!worker||xrtNetWorkerEngine(worker)!=scope.engine)xrtSetErrorKind(XERR_ARGUMENT);
            else native=xrtNetStreamRef(c->accepted);
        }else{
            xnetstreamconfig config;if(c->stream)config=*c->stream;else xrtNetStreamConfigInit(&config);
            native=xrtNetStreamConnect(scope.engine,c->remote,c->worker,&config,NULL,NULL);
        }
    }
    xrtAtomicPtrStore(&r->native,native,XMEMORY_RELEASE);xl6ExecutionResourceLeaveV1(&scope);
    return native!=NULL||xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE)!=0;
}
static bool listenerCreate(Resource* base,const XL6_ExecutionCreateOptionsV1* options)
{
    Listener* r=(Listener*)base;const XL6_TcpCreateOptionsV1* c=options->value.tcp;
    xrtAtomicPtrInit(&r->native,NULL);
    if(!c||!c->listener){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionScopeV1 scope={0};if(!engineEnter(c->engine,&scope))return false;
    r->parent=xl6ExecutionResourceRetainV1(c->engine);xnetlistener* native=NULL;
    if(r->parent){
        xnetlistenconfig config=*c->listener;
        native=xrtNetListen(scope.engine,&config,NULL,NULL,NULL);
    }
    xrtAtomicPtrStore(&r->native,native,XMEMORY_RELEASE);xl6ExecutionResourceLeaveV1(&scope);
    return native!=NULL;
}
/* Native callbacks never borrow the language record. On failed publication,
 * abort is allocation-free; the native runtime slot owns any dispatch tail.
 * Releasing this creator and the parent owner cannot leave a dangling Data. */
static bool streamRollback(Resource* base)
{
    Stream* r=(Stream*)base;xnetstream* native=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    xfuture* pending=xrtAtomicPtrExchange(&r->pending,NULL,XMEMORY_ACQ_REL);
    xrtAtomic32Store(&r->prepared,0,XMEMORY_RELEASE);
    if(pending){
        /* Construction never submits Accept. Ordinary retirement retains an
         * in-flight Future through its REAL terminal state before reaching
         * here: cancellation after native unlink can still deliver a Stream. */
        if(xrtFutureState(pending)==XFUTURE_PENDING)abort();
        if(xrtFutureState(pending)==XFUTURE_RESOLVED){xnetstream* undelivered=xrtFutureValue(pending);
            if(undelivered)(void)xrtNetStreamAbort(undelivered);}
        xrtFutureDestroy(pending);
    }
    if(native){(void)xrtNetStreamAbort(native);xrtNetStreamDestroy(native);}
    parentDrop(&r->accepting);parentDrop(&r->parent);return true;
}
static bool listenerRollback(Resource* base)
{
    Listener* r=(Listener*)base;xnetlistener* native=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    if(native){(void)xrtNetListenerClose(native);xrtNetListenerDestroy(native);}parentDrop(&r->parent);return true;
}
/* Last-owner cleanup does not promise delivery or implicitly flush forever.
 * Explicit Close drains; callers wait for that boundary before losing owners
 * when delivery matters. Automatic retirement aborts remaining stream work. */
static XL6_ModulePrepareResult streamRetire(Resource* base)
{
    Stream* r=(Stream*)base;xnetstream* native=stream(base);
    if(!native){
        xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
        if(pending&&xrtFutureState(pending)==XFUTURE_PENDING){
            (void)xrtFutureCancel(pending);
            if(xrtFutureState(pending)==XFUTURE_PENDING)return XL6_MODULE_PREPARE_BUSY;
        }
        return streamRollback(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
    }
    if(!xrtNetStreamAbort(native))return XL6_MODULE_PREPARE_FAILED;
    if(xrtNetStreamState(native)!=XNET_STREAM_CLOSED)return XL6_MODULE_PREPARE_BUSY;
    return streamRollback(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
static XL6_ModulePrepareResult listenerRetire(Resource* base)
{
    xnetlistener* native=listener(base);if(!native)return XL6_MODULE_PREPARE_READY;
    if(!xrtNetListenerClose(native))return XL6_MODULE_PREPARE_FAILED;
    if(xrtNetListenerState(native)!=XNET_LISTENER_CLOSED)return XL6_MODULE_PREPARE_BUSY;
    return listenerRollback(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
/* Clear has already prepared the real native child; no new close is admitted
 * through that child after Clear. Only consume our native creator and parent. */
static bool streamFinish(Resource* base)
{
    Stream* r=(Stream*)base;xnetstream* native=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    xfuture* pending=xrtAtomicPtrExchange(&r->pending,NULL,XMEMORY_ACQ_REL);
    xrtAtomic32Store(&r->prepared,0,XMEMORY_RELEASE);
    xrtFutureDestroy(pending);xrtNetStreamDestroy(native);parentDrop(&r->accepting);parentDrop(&r->parent);return true;
}
static bool listenerFinish(Resource* base)
{
    Listener* r=(Listener*)base;xnetlistener* native=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    xrtNetListenerDestroy(native);parentDrop(&r->parent);return true;
}
static void streamDrop(Resource* base)
{Stream* r=(Stream*)base;if(stream(base)||xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE)||
    xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE)||r->parent||r->accepting)abort();}
static void listenerDrop(Resource* base)
{Listener* r=(Listener*)base;if(listener(base)||r->parent)abort();}
static bool streamTrace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const Stream* r=(const Stream*)base;xnetstream* native=stream(base);
    xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
    return (!native||visit(xrtNetStreamOwnership(native),context))&&
        (!pending||visit(xrtFutureOwnership(pending),context))&&
        (!r->accepting||visit(xl6ExecutionResourceOwnershipV1(r->accepting),context))&&
        (!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context));
}
static bool listenerTrace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const Listener* r=(const Listener*)base;xnetlistener* native=listener(base);
    return (!native||visit(xrtNetListenerOwnership(native),context))&&
        (!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context));
}
static bool streamClose(Resource* base)
{
    xnetstream* s=stream(base);if(s)return xrtNetStreamClose(s);
    xfuture* pending=xrtAtomicPtrLoad(&((Stream*)base)->pending,XMEMORY_ACQUIRE);
    if(pending&&xrtFutureState(pending)==XFUTURE_PENDING)(void)xrtFutureCancel(pending);
    return true;
}
static bool streamCancel(Resource* base)
{xnetstream* s=stream(base);return s?xrtNetStreamAbort(s):streamClose(base);}
static bool listenerClose(Resource* base){xnetlistener* l=listener(base);return !l||xrtNetListenerClose(l);}
static bool streamPresent(const Resource* r)
{return stream(r)!=NULL||xrtAtomicPtrLoad(&((const Stream*)r)->pending,XMEMORY_ACQUIRE)!=NULL||
    xrtAtomic32Load(&((const Stream*)r)->prepared,XMEMORY_ACQUIRE)!=0;}
static bool listenerPresent(const Resource* r){return listener(r)!=NULL;}
const XL6_ExecutionFamilyV1* xl6ExecutionTcpStreamFamilyV1Get(void)
{
    static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,
        XL6_EXECUTION_TCP_STREAM_V1,streamCreate,streamRollback,streamRetire,streamFinish,streamDrop,
        streamTrace,streamClose,streamCancel,sizeof(Stream),streamPresent};return &family;
}
const XL6_ExecutionFamilyV1* xl6ExecutionTcpListenerFamilyV1Get(void)
{
    static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,
        XL6_EXECUTION_TCP_LISTENER_V1,listenerCreate,listenerRollback,listenerRetire,listenerFinish,listenerDrop,
        listenerTrace,listenerClose,listenerClose,sizeof(Listener),listenerPresent};return &family;
}
Resource* xl6ExecutionTcpConnectV1(XL6_ExecutionSourceV1* source,Resource* engine,
    const xnetaddr* remote,uint32_t worker,const xnetstreamconfig* config)
{
    XL6_TcpCreateOptionsV1 tcp={.engine=engine,.remote=remote,.worker=worker,.stream=config};
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TCP_STREAM_V1,.value.tcp=&tcp};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionTcpStreamFamilyV1Get(),&options);
}
Resource* xl6ExecutionTcpAdoptV1(XL6_ExecutionSourceV1* source,Resource* engine,xnetstream* accepted)
{
    if(!accepted){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_TcpCreateOptionsV1 tcp={.engine=engine,.accepted=accepted};
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TCP_STREAM_V1,.value.tcp=&tcp};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionTcpStreamFamilyV1Get(),&options);
}
Resource* xl6ExecutionTcpListenV1(XL6_ExecutionSourceV1* source,Resource* engine,const xnetlistenconfig* config)
{
    XL6_TcpCreateOptionsV1 tcp={.engine=engine,.listener=config};
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TCP_LISTENER_V1,.value.tcp=&tcp};
    return xl6ExecutionCreateInternalV1(source,xl6ExecutionTcpListenerFamilyV1Get(),&options);
}
Resource* xl6ExecutionTcpPrepareAcceptV1(XL6_ExecutionSourceV1* source,Resource* receiver)
{
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(receiver,XL6_EXECUTION_TCP_LISTENER_V1,true,&scope))return NULL;
    XL6_TcpCreateOptionsV1 tcp={.engine=((Listener*)receiver)->parent,.accepting=receiver};
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TCP_STREAM_V1,.value.tcp=&tcp};
    Resource* result=xl6ExecutionCreateInternalV1(source,xl6ExecutionTcpStreamFamilyV1Get(),&options);
    xl6ExecutionTcpLeaveInternalV1(&scope);return result;
}
static xfuture* acceptSource(Resource* base,Resource* expected)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceControlV1(base,true,&scope))return NULL;
    xfuture* result=NULL;
    if(base->family!=xl6ExecutionTcpStreamFamilyV1Get())xrtSetErrorKind(XERR_TYPE);
    else{
        Stream* r=(Stream*)base;
        xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
        if(expected&&r->accepting!=expected)xrtSetErrorKind(XERR_STATE);
        else if(!pending&&xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE)){
            XL6_TcpScopeV1 accepting={0};
            if(xl6ExecutionTcpEnterInternalV1(r->accepting,XL6_EXECUTION_TCP_LISTENER_V1,true,&accepting)){
                pending=xrtNetListenerAcceptAsync(accepting.listener);
                if(pending){xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
                    xrtAtomicPtrStore(&r->pending,pending,XMEMORY_RELEASE);xrtAtomic32Store(&r->prepared,0,XMEMORY_RELEASE);
                    if(!xrtOwnershipScopeEnd(&mutation))abort();}
                xl6ExecutionTcpLeaveInternalV1(&accepting);
            }
        }
        if(expected&&r->accepting!=expected)result=NULL;
        else if(pending)result=xrtFutureRef(pending);else if(!xrtGetError())xrtSetErrorKind(XERR_STATE);
    }
    xl6ExecutionResourceLeaveV1(&scope);return result;
}
xfuture* xl6ExecutionTcpAcceptSourceV1(Resource* base){return acceptSource(base,NULL);}
xfuture* xl6ExecutionTcpAcceptSourceForV1(Resource* base,Resource* listener)
{if(!listener){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}return acceptSource(base,listener);}
bool xl6ExecutionTcpPromoteAcceptV1(Resource* base)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceControlV1(base,true,&scope))return false;
    bool ok=false;
    if(base->family!=xl6ExecutionTcpStreamFamilyV1Get())xrtSetErrorKind(XERR_TYPE);
    else{
        Stream* r=(Stream*)base;xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
        if(!pending||stream(base)||xrtFutureState(pending)!=XFUTURE_RESOLVED)xrtSetErrorKind(XERR_STATE);
        else{
            xnetstream* owned=xrtNetStreamRef(xrtFutureValue(pending));
            if(owned){xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
                xrtAtomicPtrStore(&r->native,owned,XMEMORY_RELEASE);xrtAtomicPtrStore(&r->pending,NULL,XMEMORY_RELEASE);
                if(!xrtOwnershipScopeEnd(&mutation))abort();
                xrtFutureDestroy(pending);parentDrop(&r->accepting);ok=true;}
        }
    }
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionTcpEnterInternalV1(Resource* r,XL6_ExecutionKindV1 kind,bool submit,XL6_TcpScopeV1* scope)
{
    if(!scope||scope->execution.resource||(kind!=XL6_EXECUTION_TCP_STREAM_V1&&kind!=XL6_EXECUTION_TCP_LISTENER_V1)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xl6ExecutionResourceEnterV1(r,submit,&scope->execution))return false;
    const XL6_ExecutionFamilyV1* expected=kind==XL6_EXECUTION_TCP_STREAM_V1?
        xl6ExecutionTcpStreamFamilyV1Get():xl6ExecutionTcpListenerFamilyV1Get();
    if(r->kind!=kind||r->family!=expected){xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_TYPE);return false;}
    if(kind==XL6_EXECUTION_TCP_STREAM_V1){scope->stream=stream(r);scope->engineOwner=((Stream*)r)->parent;}
    else{scope->listener=listener(r);scope->engineOwner=((Listener*)r)->parent;}
    if(!scope->stream&&!scope->listener){scope->engineOwner=NULL;xl6ExecutionResourceLeaveV1(&scope->execution);xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
void xl6ExecutionTcpLeaveInternalV1(XL6_TcpScopeV1* scope)
{xl6ExecutionResourceLeaveV1(&scope->execution);scope->stream=NULL;scope->listener=NULL;scope->engineOwner=NULL;}
bool xl6ExecutionTcpStateV1(Resource* r,int* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!r){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(r,r->kind,false,&scope))return false;
    *output=scope.stream?(int)xrtNetStreamState(scope.stream):(int)xrtNetListenerState(scope.listener);
    xl6ExecutionTcpLeaveInternalV1(&scope);return true;
}
bool xl6ExecutionTcpLocalV1(Resource* r,xnetaddr* output)
{
    if(!output||!r){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(r,r->kind,false,&scope))return false;
    bool ok=scope.stream?xrtNetStreamLocal(scope.stream,output):xrtNetListenerLocal(scope.listener,output);
    xl6ExecutionTcpLeaveInternalV1(&scope);return ok;
}
bool xl6ExecutionTcpRemoteV1(Resource* r,xnetaddr* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(r,XL6_EXECUTION_TCP_STREAM_V1,false,&scope))return false;
    bool ok=xrtNetStreamRemote(scope.stream,output);xl6ExecutionTcpLeaveInternalV1(&scope);return ok;
}
bool xl6ExecutionTcpShutdownWriteV1(Resource* r)
{
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(r,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return false;
    bool ok=xrtNetStreamShutdownWrite(scope.stream);xl6ExecutionTcpLeaveInternalV1(&scope);return ok;
}
bool xl6ExecutionTcpPauseReadV1(Resource* r,bool paused)
{
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(r,XL6_EXECUTION_TCP_STREAM_V1,true,&scope))return false;
    bool ok=true;if(paused)xrtNetStreamPause(scope.stream);else ok=xrtNetStreamResume(scope.stream);
    xl6ExecutionTcpLeaveInternalV1(&scope);return ok;
}
bool xl6ExecutionTcpStreamStatsV1(Resource* r,xnetstreamstats* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(r,XL6_EXECUTION_TCP_STREAM_V1,false,&scope))return false;
    bool ok=xrtNetStreamStats(scope.stream,output);xl6ExecutionTcpLeaveInternalV1(&scope);return ok;
}
bool xl6ExecutionTcpListenerStatsV1(Resource* r,xnetlistenerstats* output)
{
    if(!output){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_TcpScopeV1 scope={0};if(!xl6ExecutionTcpEnterInternalV1(r,XL6_EXECUTION_TCP_LISTENER_V1,false,&scope))return false;
    bool ok=xrtNetListenerStats(scope.listener,output);xl6ExecutionTcpLeaveInternalV1(&scope);return ok;
}
