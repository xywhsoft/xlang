#include "include/xruntime_decl.h"
#include "xl6_net_socket_owner.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>
struct XL6_NetSocketOwnerV1 {
    xatomic32 gate;
    size_t references,active;
    xnetsocket socket,retired;
    const void* claim;
    bool closing,control,cleared,finished;
};
typedef XL6_NetSocketOwnerV1 Owner;
static const xrtownershipops ownershipOps;
static void enter(Owner* owner,xrtownershipscope* mutation)
{
    if(!xrtOwnershipMutationBegin(mutation))abort();
    for(;;){uint32_t available=0;
        if(xrtAtomic32CompareExchange(&owner->gate,&available,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();}
}
static void leave(Owner* owner,xrtownershipscope* mutation)
{xrtAtomic32Store(&owner->gate,0,XMEMORY_RELEASE);if(!xrtOwnershipScopeEnd(mutation))abort();}
XL6_NetSocketOwnerV1* xl6NetSocketOwnerEmptyV1(void)
{
    Owner* owner=xrtCalloc(1,sizeof(*owner));if(!owner)return NULL;
    owner->references=1;return owner;
}
XL6_NetSocketOwnerV1* xl6NetSocketOwnerOpenV1(int family,int kind,bool nonblocking)
{
    Owner* owner=xl6NetSocketOwnerEmptyV1();if(!owner)return NULL;
    owner->socket=xrtNetSocketOpen((xnetfamily)family,(xnetsockettype)kind,nonblocking?XNET_SOCKET_NONBLOCK:0);
    if(!owner->socket){xrtFree(owner);return NULL;}return owner;
}
Owner* xl6NetSocketOwnerRetainV1(Owner* owner)
{
    if(!owner){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xrtownershipscope mutation={0};enter(owner,&mutation);
    bool ok=owner->references&&owner->references<SIZE_MAX&&!owner->claim&&!owner->cleared;
    if(ok)++owner->references;
    leave(owner,&mutation);
    if(!ok)xrtSetErrorKind(XERR_STATE);
    return ok?owner:NULL;
}
void xl6NetSocketOwnerReleaseV1(Owner* owner)
{
    if(!owner)return;
    xrtownershipscope mutation={0};enter(owner,&mutation);if(!owner->references)abort();
    bool last=--owner->references==0;xnetsocket native=NULL;
    if(last){
        if(owner->active||owner->closing||(owner->claim&&!owner->cleared)||owner->retired||
            (owner->cleared&&!owner->finished))abort();
        native=owner->socket;owner->socket=NULL;
    }
    leave(owner,&mutation);
    if(last){bool ok=!native||xrtNetSocketClose(native);xrtFree(owner);
        if(!ok)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
}
static bool operationEnter(Owner* owner,bool control,XL6_NetSocketScopeV1* scope)
{
    if(!owner||!scope||scope->owner){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};enter(owner,&mutation);
    bool ok=owner->references&&owner->references<SIZE_MAX&&owner->active<SIZE_MAX&&
        owner->socket&&!owner->claim&&!owner->cleared&&!owner->closing&&!owner->control&&(!control||!owner->active);
    xerrkind reason=owner->claim||owner->closing||owner->control||(control&&owner->active)?XERR_STATE:
        owner->references==SIZE_MAX||owner->active==SIZE_MAX?XERR_RANGE:XERR_CLOSED;
    if(ok){++owner->references;++owner->active;owner->control=control;
        *scope=(XL6_NetSocketScopeV1){owner,owner->socket,xrtThreadCurrentId(),control};}
    leave(owner,&mutation);if(!ok)xrtSetErrorKind(reason);return ok;
}
bool xl6NetSocketOwnerEnterV1(Owner* owner,XL6_NetSocketScopeV1* scope){return operationEnter(owner,false,scope);}
bool xl6NetSocketOwnerControlV1(Owner* owner,XL6_NetSocketScopeV1* scope){return operationEnter(owner,true,scope);}
int xl6NetSocketOwnerAcceptV1(Owner* listener,Owner* destination)
{
    if(!listener||!destination||listener==destination){xrtSetErrorKind(XERR_ARGUMENT);return XNET_RESULT_ERROR;}
    XL6_NetSocketScopeV1 source={0},target={0};
    if(!xl6NetSocketOwnerEnterV1(listener,&source))return XNET_RESULT_ERROR;
    xrtownershipscope mutation={0};enter(destination,&mutation);
    bool ready=destination->references&&destination->references<SIZE_MAX&&!destination->active&&
        !destination->socket&&!destination->retired&&!destination->claim&&!destination->cleared&&!destination->closing;
    if(ready){++destination->references;destination->active=1;destination->control=true;
        target=(XL6_NetSocketScopeV1){destination,NULL,xrtThreadCurrentId(),true};}
    leave(destination,&mutation);
    if(!ready){xl6NetSocketOwnerLeaveV1(&source);xrtSetErrorKind(XERR_STATE);return XNET_RESULT_ERROR;}
    xnetsocket native=NULL;xnetresult result=xrtNetSocketAccept(source.native,&native,NULL);
    if(result==XNET_RESULT_OK){
        if(!native)abort();
        enter(destination,&mutation);destination->socket=native;leave(destination,&mutation);
    }else if(native)abort(); /* XRT success-only output ownership. */
    /* The operation reference keeps both actual endpoints alive through the
     * entire native ownership transfer, including last-language-owner races. */
    if(result==XNET_RESULT_ERROR)xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});
    xl6NetSocketOwnerLeaveV1(&target);xl6NetSocketOwnerLeaveV1(&source);return result;
}
void xl6NetSocketOwnerLeaveV1(XL6_NetSocketScopeV1* scope)
{
    if(!scope||!scope->owner||scope->thread!=xrtThreadCurrentId())abort();
    Owner* owner=scope->owner;xrtownershipscope mutation={0};enter(owner,&mutation);
    if(!owner->active)abort();
    if(scope->control){if(!owner->control||owner->active!=1)abort();owner->control=false;}
    --owner->active;*scope=(XL6_NetSocketScopeV1){0};leave(owner,&mutation);
    xl6NetSocketOwnerReleaseV1(owner);
}
bool xl6NetSocketOwnerCloseV1(Owner* owner)
{
    if(!owner){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};enter(owner,&mutation);
    if(!owner->references||owner->references==SIZE_MAX||owner->claim||owner->cleared||owner->closing||owner->active){
        leave(owner,&mutation);xrtSetErrorKind(XERR_STATE);return false;}
    xnetsocket native=owner->socket;
    if(!native){leave(owner,&mutation);return true;}
    owner->socket=NULL;owner->closing=true;++owner->references;leave(owner,&mutation);
    bool ok=xrtNetSocketClose(native);
    enter(owner,&mutation);owner->closing=false;leave(owner,&mutation);xl6NetSocketOwnerReleaseV1(owner);return ok;
}
bool xl6NetSocketOwnerClosedV1(const Owner* raw)
{
    if(!raw)return true;
    Owner* owner=(Owner*)raw;xrtownershipscope mutation={0};enter(owner,&mutation);
    bool closed=!owner->socket;leave(owner,&mutation);return closed;
}
static bool count(const void* raw,size_t* output)
{
    const Owner* owner=raw;
    if(!owner||!output||!owner->references||owner->active||owner->closing||owner->cleared)return false;
    *output=owner->references;return true;
}
static bool trace(const void* raw,xrtownershipvisitor visit,void* context)
{size_t refs;(void)context;return visit&&count(raw,&refs); /* OS handle, no owning language edges. */}
static const xrtownershipops ownershipOps={count,trace};
struct xrtownershipref xl6NetSocketOwnershipV1(const Owner* owner)
{return (xrtownershipref){owner,owner?&ownershipOps:NULL};}
static bool busy(const void* raw)
{const Owner* owner=raw;return owner&&owner->references&&!owner->cleared&&(owner->active||owner->closing);}
const xrtownershipquiescencev1* xl6NetSocketQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&ownershipOps,busy};return &policy;}
static bool hold(const void* raw)
{
    Owner* owner=(Owner*)raw;
    if(!owner->references||owner->references==SIZE_MAX||owner->cleared)return false;
    ++owner->references;return true;
}
static void drop(const void* raw){xl6NetSocketOwnerReleaseV1((Owner*)raw);}
static bool claim(const void* raw,const void* token)
{
    Owner* owner=(Owner*)raw;
    if(!token||owner->cleared||owner->active||owner->closing||(owner->claim&&owner->claim!=token))return false;
    owner->claim=token;return true;
}
static void restore(const void* raw,const void* token)
{Owner* owner=(Owner*)raw;if(owner->claim!=token||owner->cleared)abort();owner->claim=NULL;}
static void clear(const void* raw,const void* token)
{
    Owner* owner=(Owner*)raw;
    if(owner->claim!=token||owner->cleared||owner->active||owner->closing||owner->retired)abort();
    owner->retired=owner->socket;owner->socket=NULL;owner->cleared=true;
}
static bool finish(const void* raw,const void* token)
{
    Owner* owner=(Owner*)raw;xrtownershipscope mutation={0};enter(owner,&mutation);
    if(owner->claim!=token||!owner->cleared)abort();
    if(owner->finished){leave(owner,&mutation);return true;}
    xnetsocket native=owner->retired;owner->retired=NULL;owner->finished=true;leave(owner,&mutation);
    return !native||xrtNetSocketClose(native);
}
const xrtownershipadapterv1* xl6NetSocketOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,finish};
    size_t refs;return ref.Ops==&ownershipOps&&count(ref.Data,&refs)?&adapter:NULL;
}
