#include "xl6_ws_stream_internal.h"
#include "xl6_native_class_borrow.h"

typedef XL6_ExecutionResourceV1 Resource;
typedef XL6_WsResource Stream;
static xwsstream* native(const Stream* r){return xrtAtomicPtrLoad(&r->native,XMEMORY_ACQUIRE);}
void xl6WsStreamFailV2(void){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
void xl6WsStreamRecordFailureV2(Stream* r,xerror* error)
{
    xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))abort();
    if(!xrtAtomic32Load(&r->failed,XMEMORY_ACQUIRE)){
        xrtAtomicPtrStore(&r->error,error,XMEMORY_RELEASE);error=NULL;
        xrtAtomic32Store(&r->failed,1,XMEMORY_RELEASE);
    }
    xrtAtomic32Store(&r->retired,1,XMEMORY_RELEASE);
    if(!xrtOwnershipScopeEnd(&m))abort();
    xrtErrorFree(error);
}
static void emit(Stream* r,xwsstream* connection,int kind,const xwsmessageinfo* info,
    xbytesview data,size_t pending,const xwsstreamclose* close,const xerror* error)
{
    if(xrtAtomic32Load(&r->retired,XMEMORY_ACQUIRE))return;
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(&r->base,false,&scope)){xrtClearError();return;}
    xl6_error_scope errors={0};bool begun=xl6ErrorScopeBegin(&errors),keep=true,ok=begun;
    void* event=NULL;
    if(ok){event=xl6WsEventCopyV2(kind,info,data,pending,close,error);ok=event!=NULL;}
    if(ok){((XL6_WsEvent*)event)->stream=xl6ExecutionResourceRetainV1(&r->base);ok=((XL6_WsEvent*)event)->stream!=NULL;}
    if(ok)ok=xl6NativeClassPredicateInvokeTakeV1(r->callback,&event,&keep);
    if(event)xl6WsEventDestroyV2(event);
    if(begun&&!ok)xl6ErrorScopeCaptureFailure(&errors);
    xl6_error_result outcome=begun?xl6ErrorScopeEnd(&errors):(xl6_error_result){false,xrtTakeError()};
    if(!outcome.ok){xl6WsStreamRecordFailureV2(r,outcome.error);(void)xrtWsStreamAbort(connection);xrtClearError();}
    else{xrtErrorFree(outcome.error);if(!keep)xrtWsStreamPause(connection);}
    xl6ExecutionResourceLeaveV1(&scope);
}
static void messageBegin(xwsstream* s,const xwsmessageinfo* info,void* data)
{Stream* r=data;r->message=*info;r->delivered=0;emit(r,s,0,info,(xbytesview){0},0,NULL,NULL);}
static void messageData(xwsstream* s,xbytesview bytes,void* data)
{
    Stream* r=data;
    if(xrtAtomic32Load(&r->retired,XMEMORY_ACQUIRE))return;
    xwsmessageinfo info=r->message;info.Offset=r->delivered;info.Flags&=~XWS_MESSAGE_BEGIN;
    if(bytes.Size>SIZE_MAX-r->delivered){xrtSetErrorKind(XERR_RANGE);xl6WsStreamRecordFailureV2(r,xrtTakeError());(void)xrtWsStreamAbort(s);return;}
    r->delivered+=bytes.Size;
    if(r->output){
        if(!xl6WsAppend(r->output,bytes)){xl6WsStreamRecordFailureV2(r,xrtTakeError());(void)xrtWsStreamAbort(s);xrtClearError();return;}
        /* Buffer owns the only payload copy; the event carries byte count. */
        if(r->outputDataEvents)emit(r,s,1,&info,(xbytesview){0},bytes.Size,NULL,NULL);
    }else emit(r,s,1,&info,bytes,0,NULL,NULL);
}
static void messageEnd(xwsstream* s,void* data)
{Stream* r=data;xwsmessageinfo info=r->message;info.Offset=r->delivered;info.Flags=(info.Flags&~XWS_MESSAGE_BEGIN)|XWS_MESSAGE_END;emit(r,s,2,&info,(xbytesview){0},0,NULL,NULL);}
static void ping(xwsstream* s,xbytesview bytes,void* data){emit(data,s,3,NULL,bytes,0,NULL,NULL);}
static void pong(xwsstream* s,xbytesview bytes,void* data){emit(data,s,4,NULL,bytes,0,NULL,NULL);}
static void backpressure(xwsstream* s,size_t n,void* data){emit(data,s,5,NULL,(xbytesview){0},n,NULL,NULL);}
static void notify(Stream* r,xwsstream* s,int kind)
{if(r->waitPolicy&&!r->waitPolicy->Notify(r,s,kind)){xl6WsStreamRecordFailureV2(r,xrtTakeError());(void)xrtWsStreamAbort(s);xrtClearError();}}
static void writable(xwsstream* s,size_t n,void* data){notify(data,s,6);emit(data,s,6,NULL,(xbytesview){0},n,NULL,NULL);}
static void drain(xwsstream* s,void* data){notify(data,s,7);emit(data,s,7,NULL,(xbytesview){0},0,NULL,NULL);}
static void failed(xwsstream* s,const xerror* error,void* data){notify(data,s,8);emit(data,s,8,NULL,(xbytesview){0},0,NULL,error);}
static void closed(xwsstream* s,const xwsstreamclose* close,void* data){notify(data,s,9);emit(data,s,9,NULL,(xbytesview){0},0,close,NULL);}
static void retireCallback(const void* data){xrtAtomic32Store(&((Stream*)data)->retired,1,XMEMORY_RELEASE);}
const xwsstreamownershipv1* xl6WsStreamDataPolicyV2Get(void)
{
    static const xwsstreamownershipv1 policy={sizeof(policy),
        {messageBegin,messageData,messageEnd,ping,pong,backpressure,writable,drain,failed,closed},
        xl6ExecutionResourceNativeDropV1,&xl6ExecutionResourceNativeOpsV1,retireCallback};return &policy;
}
static bool create(Resource* base,const XL6_ExecutionCreateOptionsV1* raw)
{
    Stream* r=(Stream*)base;const XL6_WsCreateOptions* o=raw->value.websocket;
    xrtAtomicPtrInit(&r->native,NULL);xrtAtomicPtrInit(&r->error,NULL);
    xrtAtomic32Init(&r->retired,0);xrtAtomic32Init(&r->graceful,0);xrtAtomic32Init(&r->failed,0);
    xrtAtomic32Init(&r->prepared,1);
    xrtAtomic32Init(&r->sweepQueued,0);
    if(!o||!o->callback){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    r->callback=xl6ClosureRuntimeV1Get()->Retain(o->callback);
    if(!r->callback)return false;
    /* Register the actual language resource BEFORE committing a transport
     * transfer. A concurrent source close can reject this preparation without
     * consuming the caller's TCP/TLS or read-ahead blocks. */
    return true;
}
static bool dispose(Resource* base)
{
    Stream* r=(Stream*)base;
    xrtAtomic32Store(&r->retired,1,XMEMORY_RELEASE);
    if(r->waitPolicy&&!r->waitPolicy->Drop(r))return false;
    xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))abort();
    xrtAtomic32Store(&r->retired,1,XMEMORY_RELEASE);
    xwsstream* s=xrtAtomicPtrExchange(&r->native,NULL,XMEMORY_ACQ_REL);
    xrtAtomic32Store(&r->prepared,0,XMEMORY_RELEASE);
    xerror* error=xrtAtomicPtrExchange(&r->error,NULL,XMEMORY_ACQ_REL);
    Resource* parent=r->parent;r->parent=NULL;
    XL6_Closure* callback=r->callback;r->callback=NULL;
    xvalue* output=r->outputOwner;r->outputOwner=NULL;r->output=NULL;
    if(!xrtOwnershipScopeEnd(&m))abort();
    xrtWsStreamDestroy(s);xrtErrorFree(error);xrtValueRelease(output);
    xl6ClosureRuntimeV1Get()->Release(callback);xl6ExecutionResourceReleaseV1(parent);return true;
}
static bool rollback(Resource* base){return native((Stream*)base)?false:dispose(base);}
static XL6_ModulePrepareResult retire(Resource* base)
{
    Stream* r=(Stream*)base;xwsstream* s=native(r);
    xrtAtomic32Store(&r->retired,1,XMEMORY_RELEASE);
    if(s&&xrtWsStreamState(s)!=XWS_STREAM_CLOSED){
        if(!xrtAtomic32Load(&r->graceful,XMEMORY_ACQUIRE)&&!xrtWsStreamAbort(s))return XL6_MODULE_PREPARE_FAILED;
        if(xrtWsStreamState(s)!=XWS_STREAM_CLOSED)return XL6_MODULE_PREPARE_BUSY;
    }
    return dispose(base)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
}
static void drop(Resource* base)
{Stream* r=(Stream*)base;if(native(r)||r->parent||r->callback||r->outputOwner||xrtAtomicPtrLoad(&r->error,XMEMORY_ACQUIRE))abort();}
static bool trace(const Resource* base,xrtownershipvisitor visit,void* context)
{
    const Stream* r=(const Stream*)base;xwsstream* s=native(r);xerror* error=xrtAtomicPtrLoad(&r->error,XMEMORY_ACQUIRE);
    return (!s||visit(xrtWsStreamOwnership(s),context))&&
        (!r->parent||visit(xl6ExecutionResourceOwnershipV1(r->parent),context))&&
        (!r->callback||visit(xl6ClosureRuntimeV1Get()->Ownership(r->callback),context))&&
        (!r->outputOwner||visit(xrtValueOwnership(r->outputOwner),context))&&
        (!error||visit(xrtErrorOwnership(error),context))&&
        (!r->waitPolicy||r->waitPolicy->Trace(r,visit,context));
}
static bool cancel(Resource* base){xwsstream* s=native((Stream*)base);return !s||xrtWsStreamState(s)==XWS_STREAM_CLOSED||xrtWsStreamAbort(s);}
static bool present(const Resource* base){const Stream* r=(const Stream*)base;return native(r)!=NULL||xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE)!=0;}
const XL6_ExecutionFamilyV1* xl6WsExecutionFamilyV2Get(void)
{
    static const XL6_ExecutionFamilyV1 family={sizeof(family),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_WS_STREAM_V2,
        create,rollback,retire,dispose,drop,trace,cancel,cancel,sizeof(Stream),present};return &family;
}
void* xl6WsStreamPrepareEventsNativeV2(XL6_ExecutionServicesV1 services,XL6_NativeClassResultContextV1 context,XL6_Closure* callback)
{
    if(context.field.policy!=xl6WsEventHandlePolicyV2Get()){
        xrtSetErrorKind(XERR_TYPE);xl6WsStreamFailV2();return NULL;}
    XL6_Closure* driver=xl6NativeClassPredicateCreateV1(context,callback);if(!driver){xl6WsStreamFailV2();return NULL;}
    XL6_WsCreateOptions ws={.callback=driver};
    XL6_ExecutionCreateOptionsV1 o={.kind=XL6_EXECUTION_WS_STREAM_V2,.value.websocket=&ws};
    Stream* result=(Stream*)xl6ExecutionCreateInternalV1(services.source,xl6WsExecutionFamilyV2Get(),&o);
    xl6ClosureRuntimeV1Get()->Release(driver);
    if(!result)xl6WsStreamFailV2();
    return result;
}
bool xl6WsStreamBindInternalV2(void* raw,void* transport,void* initial,void* upgrade,void* options,
    xwsstream* (*build)(Stream*,const XL6_WsCreateOptions*))
{
    if(!raw||!transport||!initial||!upgrade||!options||!build||!((XL6_WsUpgrade*)upgrade)->ready){
        xrtSetErrorKind(XERR_STATE);return false;}
    Stream* result=raw;
    bool ok=false;XL6_ExecutionScopeV1 scope={0};
    if(xl6ExecutionResourceControlV1(&result->base,true,&scope)){
        if(result->base.family!=xl6WsExecutionFamilyV2Get()||native(result)||
           !xrtAtomic32Load(&result->prepared,XMEMORY_ACQUIRE)){
            xl6ExecutionResourceLeaveV1(&scope);xrtSetErrorKind(XERR_STATE);return false;
        }
        XL6_WsCreateOptions ws={.transport=transport,.initial=initial,.upgrade=upgrade,.config=options};
        xwsstream* connection=build(result,&ws);
        xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
        xrtAtomicPtrStore(&result->native,connection,XMEMORY_RELEASE);
        if(connection)xrtAtomic32Store(&result->prepared,0,XMEMORY_RELEASE);
        XL6_ExecutionResourceV1* rollbackParent=connection?NULL:result->parent;
        if(!connection)result->parent=NULL;
        if(!xrtOwnershipScopeEnd(&mutation))abort();
        xl6ExecutionResourceReleaseV1(rollbackParent);
        ok=connection!=NULL;xl6ExecutionResourceLeaveV1(&scope);
    }
    return ok;
}
bool xl6WsStreamEnterV2(void* raw,bool worker,XL6_ExecutionScopeV1* scope,xwsstream** stream)
{
    if(!xl6ExecutionResourceEnterV1(raw,false,scope)){xl6WsStreamFailV2();return false;}
    Stream* r=raw;*stream=native(r);
    if(r->base.family!=xl6WsExecutionFamilyV2Get()||!*stream||(worker&&!xrtNetWorkerIsCurrent(xrtWsStreamWorker(*stream)))){
        xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_STATE);xl6WsStreamFailV2();return false;}
    return true;
}
void xl6WsStreamDestroyV2(void* r){xl6ExecutionResourceReleaseV1(r);}
static bool handleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&
        ops==xl6WsStreamHandlePolicyV2Get()->Ops&&visit(xl6ExecutionResourceOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6WsStreamHandlePolicyV2Get,xl6WsStreamDestroyV2,handleTrace)
#define WS_SCOPED(Return,Name,Worker,Expr,Default) Return xl6WsStream##Name##V2(void* raw) { \
    XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,Worker,&scope,&s))return Default; \
    Return result=(Expr);xl6ExecutionResourceLeaveV1(&scope);return result;}
WS_SCOPED(int64_t,State,false,xrtWsStreamState(s),0)
WS_SCOPED(int64_t,Role,false,xrtWsStreamRole(s),0)
WS_SCOPED(uint64_t,Pending,false,xrtWsStreamPending(s),0)
WS_SCOPED(uint64_t,Writable,false,xrtWsStreamWritable(s),0)
WS_SCOPED(uint64_t,WriteLimit,false,xrtWsStreamWriteLimit(s),0)
WS_SCOPED(bool,Paused,false,xrtWsStreamPaused(s),false)
WS_SCOPED(uint64_t,Worker,false,((Stream*)raw)->worker,0)
#undef WS_SCOPED
uint64_t xl6WsStreamRequiredSizeV2(void* raw,int64_t payload)
{
 size_t bytes,required=0;if(!xl6WsSize(payload,&bytes)){xl6WsStreamFailV2();return 0;}
 XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,false,&scope,&s))return 0;
 bool ok=xrtWsStreamSendSize(s,bytes,&required);xl6ExecutionResourceLeaveV1(&scope);if(!ok)xl6WsStreamFailV2();return required;
}
uint64_t xl6WsStreamRequiredSizeTextV2(void* r,xstrview text,int64_t size)
{xbytesview v=xl6WsAsBytes(text);if(!xl6BufferSelect(&v,size)){xl6WsStreamFailV2();return 0;}return xl6WsStreamRequiredSizeV2(r,(int64_t)v.Size);}
uint64_t xl6WsStreamRequiredSizeSpanV2(void* r,xbytesview v,int64_t size)
{if(!xl6BufferSelect(&v,size)){xl6WsStreamFailV2();return 0;}return xl6WsStreamRequiredSizeV2(r,(int64_t)v.Size);}
uint64_t xl6WsStreamRequiredSizeBytesV2(void* r,xvalue* bytes,int64_t size)
{xbytesview v;if(!xl6BufferBytesInput(bytes,size,&v)){xl6WsStreamFailV2();return 0;}return xl6WsStreamRequiredSizeV2(r,(int64_t)v.Size);}
uint64_t xl6WsStreamRequiredSizeBufferV2(void* r,void* buffer,int64_t size)
{xbytesview v;if(!xl6BufferInput(buffer,size,&v)){xl6WsStreamFailV2();return 0;}return xl6WsStreamRequiredSizeV2(r,(int64_t)v.Size);}
void* xl6WsStreamEngineV2(void* raw)
{XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,false,&scope,&s))return NULL;
 void* result=xl6ExecutionResourceRetainV1(((Stream*)raw)->parent);xl6ExecutionResourceLeaveV1(&scope);if(!result)xl6WsStreamFailV2();return result;}
xstrview xl6WsStreamProtocolV2(void* raw)
{XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,false,&scope,&s))return xl6TextFailure;
 xstrview result=xl6TextCopy(xrtWsStreamProtocol(s));xl6ExecutionResourceLeaveV1(&scope);return result;}
void* xl6WsStreamCloseInfoV2(void* raw)
{XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,false,&scope,&s))return NULL;
 xwsstreamclose info;XL6_WsEvent* result=xrtWsStreamCloseInfo(s,&info)?xl6WsEventCopyV2(9,NULL,(xbytesview){0},0,&info,NULL):NULL;
 if(result){result->stream=xl6ExecutionResourceRetainV1(raw);if(!result->stream){xl6WsEventDestroyV2(result);result=NULL;}}
 xl6ExecutionResourceLeaveV1(&scope);if(!result)xl6WsStreamFailV2();return result;}
void xl6WsStreamThrowIfFailedV2(void* raw)
{XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,false,&scope,&s))return;
 Stream* r=raw;bool bad=xrtAtomic32Load(&r->failed,XMEMORY_ACQUIRE)!=0;
 const xerror* error=bad?xrtAtomicPtrLoad(&r->error,XMEMORY_ACQUIRE):xrtWsStreamError(s);
 if(error)xrtSetError(error);
 xl6ExecutionResourceLeaveV1(&scope);if(bad||error)xl6WsStreamFailV2();}
void xl6WsStreamPauseV2(void* raw)
{XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,true,&scope,&s))return;xrtWsStreamPause(s);xl6ExecutionResourceLeaveV1(&scope);}
void xl6WsStreamResumeV2(void* raw)
{XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,true,&scope,&s))return;bool ok=xrtWsStreamResume(s);xl6ExecutionResourceLeaveV1(&scope);if(!ok)xl6WsStreamFailV2();}
void xl6WsStreamAbortV2(void* raw)
{XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,false,&scope,&s))return;
 xrtAtomic32Store(&((Stream*)raw)->graceful,0,XMEMORY_RELEASE);bool ok=xrtWsStreamState(s)==XWS_STREAM_CLOSED||xrtWsStreamAbort(s);xl6ExecutionResourceLeaveV1(&scope);if(!ok)xl6WsStreamFailV2();}
int64_t xl6WsStreamCloseV2(void* raw,int64_t code,xstrview reason,int64_t size)
{uint16 actual;xbytesview bytes=xl6WsAsBytes(reason);if(!xl6WsU16(code,&actual)||!xl6BufferSelect(&bytes,size)){xl6WsStreamFailV2();return XNET_RESULT_ERROR;}reason=xl6WsAsText(bytes);
 XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,true,&scope,&s))return XNET_RESULT_ERROR;
 xnetresult result=xrtWsStreamClose(s,actual,reason);if(result==XNET_RESULT_OK)xrtAtomic32Store(&((Stream*)raw)->graceful,1,XMEMORY_RELEASE);
 xl6ExecutionResourceLeaveV1(&scope);if(result==XNET_RESULT_ERROR)xl6WsStreamFailV2();return result;}

/* Compile-time typed adapters. Each call goes straight to its XRT operation. */
#include "xl6_ws_stream_send_impl.h"
WS_SEND(SendText,xrtWsStreamText(s,xl6WsAsText(bytes)))
WS_SEND(SendBinary,xrtWsStreamBinary(s,bytes))
WS_SEND(Ping,xrtWsStreamPing(s,bytes))
WS_SEND(Pong,xrtWsStreamPong(s,bytes))

void xl6WsStreamReceiveIntoV2(XL6_NativeClassFieldContextV1 c,void* raw,const xvalue* target,bool events)
{
 XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,true,&scope,&s))return;
 void* buffer=NULL;xvalue* owner=xl6NativeClassFieldRetainV1(c,target,xl6BufferHandlePolicyV1Get(),&buffer);
 if(owner){Stream* r=raw;xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))abort();
  xvalue* old=r->outputOwner;r->outputOwner=owner;r->output=buffer;r->outputDataEvents=events;if(!xrtOwnershipScopeEnd(&m))abort();xrtValueRelease(old);}
 xl6ExecutionResourceLeaveV1(&scope);if(!owner)xl6WsStreamFailV2();
}
void xl6WsStreamReceiveEventsV2(void* raw)
{
 XL6_ExecutionScopeV1 scope={0};xwsstream* s=NULL;if(!xl6WsStreamEnterV2(raw,true,&scope,&s))return;
 Stream* r=raw;xrtownershipscope m={0};if(!xrtOwnershipMutationBegin(&m))abort();
 xvalue* old=r->outputOwner;r->outputOwner=NULL;r->output=NULL;if(!xrtOwnershipScopeEnd(&m))abort();
 xrtValueRelease(old);xl6ExecutionResourceLeaveV1(&scope);
}
