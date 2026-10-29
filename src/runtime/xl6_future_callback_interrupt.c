#include "xl6_future_callback_interrupt.h"
#include "xl6_future_callback_internal.h"
#include "xl6_ownership_scope.h"

typedef XL6_FutureCallbackFrameV1 Frame;
typedef enum Stop {OPEN,PARENT,TIMEOUT,FAILED,SEALED} Stop;
typedef struct Interrupt {
    XL6_DeadlineTicketV1 ticket;
    xcancelwatch* parent;
    XL6_DeadlineV1 due;
    xerror* error;
    Stop stop;
    bool armed,started;
} Interrupt;
static const XL6_CallbackInterruptOpsV1 ops;
static Interrupt* state(const Frame* frame)
{return frame&&frame->interruptOps==&ops?frame->interrupt:NULL;}
static bool retain(Frame* frame)
{
    xrtownershipscope mutation={0};xl6CallbackFrameBeginV1(frame,&mutation);
    bool ok=frame->references&&frame->references<SIZE_MAX&&!frame->cleared;
    if(ok)++frame->references;
    xl6CallbackFrameEndV1(frame,&mutation);if(!ok)xrtSetErrorKind(XERR_RANGE);return ok;
}
static void drop(const void* data){xl6CallbackFrameReleaseV1((Frame*)data);}
static xrtownershipref ownership(const void* data)
{return (xrtownershipref){data,&xl6FutureCallbackFrameOpsV1};}
static void stop(Frame* frame,Stop reason,const xerror* error)
{
    xerror* previous=xrtTakeError();xrtownershipscope mutation={0};xfuture* source=NULL;
    xl6CallbackFrameBeginV1(frame,&mutation);Interrupt* interrupt=state(frame);if(!interrupt)abort();
    ++frame->active;
    if(interrupt->stop==OPEN){interrupt->stop=reason;interrupt->error=xrtErrorRef(error);source=xrtFutureRef(frame->source);}
    xl6CallbackFrameEndV1(frame,&mutation);
    if(source){(void)xrtFutureCancel(source);xrtFutureDestroy(source);}
    xl6CallbackFrameBeginV1(frame,&mutation);--frame->active;xl6CallbackFrameEndV1(frame,&mutation);
    xrtClearError();xrtSetErrorTake(previous);
}
static void cancelled(void* data){stop(data,PARENT,NULL);}
static const xcancelwatchownershipv1 cancelPolicy={sizeof(cancelPolicy),cancelled,drop,&xl6FutureCallbackFrameOpsV1};
const xcancelwatchownershipv1* xl6FutureCallbackInterruptCancelPolicyV1Get(void){return &cancelPolicy;}
static void expired(const void* data,XL6_DeadlineResultV1 result,const xerror* error)
{
    if(result!=XL6_DEADLINE_EXPIRED_V1&&result!=XL6_DEADLINE_FAILED_V1)abort();
    stop((Frame*)data,result==XL6_DEADLINE_EXPIRED_V1?TIMEOUT:FAILED,error);
}
static const XL6_DeadlineTargetV1 target={sizeof(target),expired,drop,ownership};
static void bound(Frame* frame)
{
    xerror* previous=xrtTakeError();xrtownershipscope mutation={0};
    xl6CallbackFrameBeginV1(frame,&mutation);Interrupt* interrupt=state(frame);if(!interrupt)abort();
    xfuture* source=interrupt->stop!=OPEN?xrtFutureRef(frame->source):NULL;
    xl6CallbackFrameEndV1(frame,&mutation);
    if(source){(void)xrtFutureCancel(source);xrtFutureDestroy(source);}
    xrtClearError();xrtSetErrorTake(previous);
}
static void detach(Frame* frame)
{
    xrtownershipscope mutation={0};xl6CallbackFrameBeginV1(frame,&mutation);
    Interrupt* interrupt=state(frame);if(!interrupt)abort();
    interrupt->stop=SEALED;xcancelwatch* parent=interrupt->parent;interrupt->parent=NULL;
    xerror* error=interrupt->error;interrupt->error=NULL;xl6CallbackFrameEndV1(frame,&mutation);
    xrtCancelUnwatch(parent);xl6DeadlineDetachV1(&interrupt->ticket);xrtErrorFree(error);
}
static void select_input(Frame* frame,xfutureresult* input,xerror** selected)
{
    xrtownershipscope mutation={0};xl6CallbackFrameBeginV1(frame,&mutation);
    Interrupt* interrupt=state(frame);if(!interrupt||interrupt->stop==SEALED)abort();
    if(interrupt->stop==OPEN&&xl6DeadlineExpiredV1(interrupt->due))interrupt->stop=TIMEOUT;
    Stop reason=interrupt->stop;interrupt->stop=SEALED;
    *selected=interrupt->error;interrupt->error=NULL;xl6CallbackFrameEndV1(frame,&mutation);
    if(reason==PARENT)*input=(xfutureresult){.State=XFUTURE_CANCELLED};
    else if(reason==TIMEOUT||reason==FAILED){
        if(!*selected){xrtSetErrorKind(reason==TIMEOUT?XERR_TIMEOUT:XERR_INTERNAL);*selected=xrtTakeError();}
        *input=(xfutureresult){.State=XFUTURE_FAILED,.Error=*selected};
    }else if(!frame->source)abort();
    detach(frame);
}
static bool trace(const Frame* frame,xrtownershipvisitor visit,void* context)
{
    const Interrupt* interrupt=state(frame);
    return interrupt&&visit(xrtCancelWatchOwnership(interrupt->parent),context)&&
        visit(xl6DeadlineTicketServiceOwnershipV1(&interrupt->ticket),context)&&visit(xrtErrorOwnership(interrupt->error),context);
}
static void destroy(Frame* frame)
{
    Interrupt* interrupt=state(frame);
    if(!interrupt||interrupt->stop!=SEALED||interrupt->parent||interrupt->error||!xl6DeadlineTicketIsIdleV1(&interrupt->ticket))abort();
    xrtFree(interrupt);frame->interrupt=NULL;frame->interruptOps=NULL;
}
static bool idle(const Frame* frame)
{
    const Interrupt* interrupt=state(frame);
    return interrupt&&interrupt->stop==SEALED&&!interrupt->parent&&!interrupt->error&&xl6DeadlineTicketIsIdleV1(&interrupt->ticket);
}
static const XL6_CallbackInterruptOpsV1 ops={bound,select_input,detach,trace,idle,destroy};
static bool arm_body(void* data,xcancel* parent,XL6_DeadlineServiceV1* service,XL6_DeadlineV1 due)
{
    Frame* frame=xl6CallbackFrameHeaderV1(data);
    if(!frame||!frame->queued||!frame->layoutV2||frame->references!=1||frame->committed||frame->publishing||
        frame->interruptOps||(!parent&&due==XL6_DEADLINE_NEVER)||(due!=XL6_DEADLINE_NEVER&&!service)){
        xrtSetErrorKind(XERR_ARGUMENT);return false;}
    Interrupt* interrupt=xrtCalloc(1,sizeof(*interrupt));if(!interrupt)return false;
    interrupt->due=due;frame->interrupt=interrupt;frame->interruptOps=&ops;
    if(parent){
        if(!retain(frame))goto failed;
        interrupt->parent=xrtCancelWatchOwnedV1(parent,frame,&cancelPolicy);
        if(!interrupt->parent){drop(frame);goto failed;}
    }
    if(due!=XL6_DEADLINE_NEVER){
        if(!retain(frame))goto failed;
        const void* owner=frame;
        if(!xl6DeadlineBindTakeV1(service,&interrupt->ticket,due,&target,&owner)){drop(frame);goto failed;}
    }
    interrupt->armed=true;return true;
failed:;
    xerror* error=xrtTakeError();detach(frame);xrtClearError();xrtSetErrorTake(error);return false;
}
bool xl6FutureCallbackInterruptArmV1(void* data,xcancel* parent,XL6_DeadlineServiceV1* service,XL6_DeadlineV1 due)
{XL6_OWNERSHIP_RETURN(bool,arm_body(data,parent,service,due));}
bool xl6FutureCallbackInterruptStoppedV1(const void* data)
{
    Frame* frame=xl6CallbackFrameHeaderV1(data);if(!state(frame))abort();
    xrtownershipscope mutation={0};xl6CallbackFrameBeginV1(frame,&mutation);Interrupt* interrupt=state(frame);
    if(!interrupt->armed||interrupt->started||interrupt->stop==SEALED)abort();
    if(interrupt->stop==OPEN&&xl6DeadlineExpiredV1(interrupt->due))interrupt->stop=TIMEOUT;
    bool stopped=interrupt->stop!=OPEN;xl6CallbackFrameEndV1(frame,&mutation);return stopped;
}
xfuture* xl6FutureCallbackInterruptStartV1(struct XL6_ContinuationExecutor* executor,void* data,xfuture* source)
{
    Frame* frame=xl6CallbackFrameHeaderV1(data);Interrupt* interrupt=state(frame);
    if(!interrupt){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xrtownershipscope mutation={0};xl6CallbackFrameBeginV1(frame,&mutation);
    bool valid=interrupt->armed&&!interrupt->started&&interrupt->stop!=SEALED&&(source||interrupt->stop!=OPEN);
    if(valid)interrupt->started=true;
    xl6CallbackFrameEndV1(frame,&mutation);
    if(!valid){xrtSetErrorKind(XERR_STATE);return NULL;}
    return xl6CallbackFrameStartInterruptedInternalV1(executor,data,source);
}
