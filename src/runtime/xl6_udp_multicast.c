#include "xl6_udp_internal.h"
#include "xl6_closure.h"
#include "xl6_future_payload.h"
#include "xl6_future_callback.h"
#include <string.h>
enum {JOIN,LEAVE,LOOP,HOPS,INTERFACE};
typedef struct Command {
    xvalue* receiver;
    xpromise* output;
    xcancel* cancel;
    xnetaddr group,interfaceAddress;
    int64_t argument;
    unsigned operation;
    bool hasInterface;
} Command;
static bool invokeOperation(void* receiver,const Command* c)
{
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(receiver,true,&scope))return false;
    bool ok=false;const xnetaddr* iface=c->hasInterface?&c->interfaceAddress:NULL;
    switch(c->operation){
    case JOIN:ok=xrtNetUdpJoin(scope.udp,&c->group,iface);break;
    case LEAVE:ok=xrtNetUdpLeave(scope.udp,&c->group,iface);break;
    case LOOP:ok=xrtNetUdpMulticastLoop(scope.udp,c->argument!=0);break;
    case HOPS:ok=xrtNetUdpMulticastHopLimit(scope.udp,(int)c->argument);break;
    case INTERFACE:ok=xrtNetUdpMulticastInterface(scope.udp,iface);break;
    default:xrtSetErrorKind(XERR_ARGUMENT);break;
    }
    xl6ExecutionUdpLeaveInternalV1(&scope);return ok;
}
static const unsigned commandLayout=1;
static bool commandValid(const void* layout,const void* raw,size_t bytes)
{
    const Command* c=raw;
    return layout==&commandLayout&&bytes==sizeof(*c)&&c&&c->receiver&&c->output&&c->cancel&&c->operation<=INTERFACE;
}
static bool commandTrace(const void* layout,const void* raw,size_t bytes,XL6_ModuleOwnershipVisit visit,void* data)
{
    const Command* c=raw;return commandValid(layout,raw,bytes)&&visit&&visit(xrtValueOwnership(c->receiver),data)&&
        visit(xrtPromiseOwnership(c->output),data)&&visit(xrtCancelOwnership(c->cancel),data);
}
static void commandDrop(void* raw)
{
    Command* c=raw;if(!c)return;
    xrtValueRelease(c->receiver);xrtPromiseDestroy(c->output);xrtCancelDestroy(c->cancel);xrtFree(c);
}
static bool commandEnvironmentDrop(const void* layout,void* raw,size_t bytes)
{
    if(!commandValid(layout,raw,bytes))abort();
    commandDrop(raw);return true;
}
static const XL6_ClosureEnvironmentOpsV1 commandOps={sizeof(commandOps),commandValid,commandTrace,commandEnvironmentDrop};
static bool commandRun(void* raw)
{
    Command* c=raw;void* receiver=NULL;const xvaluehandleops* ops=NULL;void* user=NULL;
    bool stopped=xrtCancelRequested(c->cancel),ok=false;bool* box=NULL;
    if(!stopped){
        ok=xrtValueGetHandle(c->receiver,&receiver,&ops,&user)&&receiver&&!user&&ops==xl6UdpHandlePolicyV1Get()->Ops;
        if(!ok)xrtSetErrorKind(XERR_TYPE);else ok=invokeOperation(receiver,c);
        if(ok){box=xrtMalloc(sizeof(*box));ok=box!=NULL;if(ok)*box=true;}
    }
    xerror* error=ok||stopped?NULL:xrtTakeError();
    if(stopped||xrtCancelRequested(c->cancel)){
        xrtFree(box);xrtErrorFree(error);(void)xrtPromiseCancel(c->output);xrtClearError();return true;
    }
    if(ok&&xl6PayloadResolveOwned(c->output,box,XL6_FUTURE_PAYLOAD_VALUE))return true;
    if(ok){error=xrtTakeError();xrtFree(box);}
    /* Cancellation may win after the final pre-publication observation. It
     * cannot undo an OS operation which already ran, and is not a job failure. */
    if(xrtCancelRequested(c->cancel)){xrtErrorFree(error);xrtClearError();return true;}
    xl6FutureCallbackFailV1(c->output,error);xrtClearError();return true;
}
static bool prepare(Command* c,unsigned op,const void* group,const void* iface,int64_t argument)
{
    if(op>INTERFACE||((op==JOIN||op==LEAVE)&&!group)||(op==HOPS&&(argument<0||argument>255))){
        xrtSetErrorKind(op==HOPS?XERR_RANGE:XERR_ARGUMENT);return false;}
    *c=(Command){.argument=argument,.operation=op,.hasInterface=iface!=NULL};
    if(group)c->group=*(const xnetaddr*)group;
    if(iface)c->interfaceAddress=*(const xnetaddr*)iface;
    return true;
}
static xfuture* start(void* receiver,unsigned op,const void* group,const void* iface,int64_t argument)
{
    Command initial={0};if(!prepare(&initial,op,group,iface,argument)){xl6UdpFailedV1();return NULL;}
    Command* c=xrtMalloc(sizeof(*c));xfuture* output=NULL;XL6_Closure* closure=NULL;bool accepted=false;
    if(!c)goto done;
    *c=initial;void* owned=xl6ExecutionResourceRetainV1(receiver);
    if(owned)c->receiver=xl6NativeHandleTakeV1(&owned,xl6UdpHandlePolicyV1Get());
    if(!c->receiver)goto done;
    c->output=xrtPromiseCreate(&output,NULL);if(!c->output)goto done;
    c->cancel=xrtPromiseCancelToken(c->output);if(!c->cancel)goto done;
    /* Permanent resident code and immutable, fully traced real owners. The
     * opaque legacy closure API is intentionally not admitted by Engine. */
    closure=xl6ClosureCreateEnvironmentV1(NULL,(void(*)(void))commandRun,NULL,c,sizeof(*c),&commandLayout,&commandOps,NULL,NULL,0);
    if(!closure)goto done;
    c=NULL;accepted=xl6ExecutionUdpPostWorkV1(receiver,closure);
done:;
    xerror* error=accepted?NULL:xrtTakeError();
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,!accepted))abort();
    xl6ClosureRuntimeV1Get()->Release(closure);commandDrop(c);if(!accepted)xrtFutureDestroy(output);
    xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
    if(!accepted){xrtErrorFree(result.error);xrtSetErrorTake(error);xl6UdpFailedV1();return NULL;}
    if(!result.ok){xrtFutureDestroy(output);xl6ErrorScopePropagateFailure(result);return NULL;}
    xrtErrorFree(result.error);return output;
}
static void sync(void* receiver,unsigned op,const void* group,const void* iface,int64_t argument)
{
    Command command={0};if(!prepare(&command,op,group,iface,argument)){xl6UdpFailedV1();return;}
    XL6_UdpScopeV1 scope={0};if(!xl6ExecutionUdpEnterInternalV1(receiver,true,&scope)){xl6UdpFailedV1();return;}
    xnetworker* worker=xrtNetUdpWorker(scope.udp);bool current=worker&&xrtNetWorkerIsCurrent(worker);
    bool foreignWorker=worker&&!current&&xrtNetEngineCurrent(xrtNetWorkerEngine(worker));
    xl6ExecutionUdpLeaveInternalV1(&scope);
    if(current){if(!invokeOperation(receiver,&command))xl6UdpFailedV1();return;}
    if(foreignWorker){xrtSetErrorKind(XERR_STATE);xl6UdpFailedV1();return;}
    xfuture* future=start(receiver,op,group,iface,argument);if(!future)return;
    xfutureresult result={0};bool ok=xrtFutureWait(future)==XWAIT_OK&&xrtFutureResult(future,&result)&&xl6PayloadObserveOutcome(result.State,result.Error);
    xerror* error=ok?NULL:xrtTakeError();xrtFutureDestroy(future);
    if(!ok){xrtClearError();xrtSetErrorTake(error);xl6UdpFailedV1();}
}
#define MEMBERSHIP(Name,Op) \
void xl6Udp##Name##V1(void* r,const void* g){sync(r,Op,g,NULL,0);} \
void xl6Udp##Name##OnV1(void* r,const void* g,const void* i){sync(r,Op,g,i,0);} \
xfuture* xl6Udp##Name##AsyncV1(void* r,const void* g){return start(r,Op,g,NULL,0);} \
xfuture* xl6Udp##Name##OnAsyncV1(void* r,const void* g,const void* i){return start(r,Op,g,i,0);}
MEMBERSHIP(MulticastJoin,JOIN)
MEMBERSHIP(MulticastLeave,LEAVE)
#undef MEMBERSHIP
void xl6UdpMulticastLoopV1(void* r,bool b){sync(r,LOOP,NULL,NULL,b);}
xfuture* xl6UdpMulticastLoopAsyncV1(void* r,bool b){return start(r,LOOP,NULL,NULL,b);}
void xl6UdpMulticastHopLimitV1(void* r,int64_t n){sync(r,HOPS,NULL,NULL,n);}
xfuture* xl6UdpMulticastHopLimitAsyncV1(void* r,int64_t n){return start(r,HOPS,NULL,NULL,n);}
void xl6UdpMulticastInterfaceV1(void* r,const void* i){sync(r,INTERFACE,NULL,i,0);}
xfuture* xl6UdpMulticastInterfaceAsyncV1(void* r,const void* i){return start(r,INTERFACE,NULL,i,0);}
void xl6UdpMulticastInterfaceDefaultV1(void* r){sync(r,INTERFACE,NULL,NULL,0);}
xfuture* xl6UdpMulticastInterfaceDefaultAsyncV1(void* r){return start(r,INTERFACE,NULL,NULL,0);}
