/* Actual native Proxy/Font participants through the resident ownership plan.
 * The component is compiled from its pinned source, not simulated adapters. */
#define XRT_MODULE_MEMORY_DEBUG
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_callback_failure.h"
#include "src/api/xl_foreign_error.h"
#include <xui.h>
#include <assert.h>
#include <stdio.h>
typedef struct Owner {
    size_t refs,drops,payloads,disposed;
    xui_font font;
    const void* claim;
    bool cleared,finished,failDrop;
} Owner;
typedef struct Payload {Owner* owner;int status;bool diagnostic;} Payload;
static const xrtownershipops ownerOps;
static bool ownerCount(const void* raw,size_t* out)
{const Owner* owner=raw;assert(owner->refs);*out=owner->refs;return true;}
static bool ownerTrace(const void* raw,xrtownershipvisitor visit,void* user)
{const Owner* owner=raw;return owner->cleared || !owner->font || visit(xuiFontOwnership(owner->font),user);}
static const xrtownershipops ownerOps={ownerCount,ownerTrace};
static bool ownerHold(const void* raw)
{Owner* owner=(void*)raw;xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));++owner->refs;assert(xrtOwnershipScopeEnd(&scope));return true;}
static bool ownerDropChecked(const void* raw)
{
    Owner* owner=(void*)raw;xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));assert(owner->refs);
    bool last=--owner->refs==0;++owner->drops;assert(xrtOwnershipScopeEnd(&scope));
    if(last){assert(!owner->font);++owner->disposed;}
    return !last || !owner->failDrop;
}
static void ownerDrop(const void* raw){assert(ownerDropChecked(raw));}
static bool ownerClaim(const void* raw,const void* token)
{Owner* owner=(void*)raw;if(owner->claim && owner->claim!=token)return false;owner->claim=token;return true;}
static void ownerRestore(const void* raw,const void* token)
{Owner* owner=(void*)raw;assert(owner->claim==token && !owner->cleared);owner->claim=NULL;}
static void ownerClear(const void* raw,const void* token)
{Owner* owner=(void*)raw;assert(owner->claim==token);owner->cleared=true;}
static xrtownershipfinishresult ownerFinish(const void* raw,const void* token)
{
    Owner* owner=(void*)raw;assert(owner->claim==token && owner->cleared);
    if(owner->finished)return XRT_OWNERSHIP_FINISH_READY;
    xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));
    xui_font font=owner->font;owner->font=NULL;owner->finished=true;assert(xrtOwnershipScopeEnd(&scope));
    if(font){bool consumed=false;int status=xuiFontReleaseOwnerChecked(font,&consumed);assert(consumed);if(status!=XUI_OK)return XRT_OWNERSHIP_FINISH_FAILED;}
    return XRT_OWNERSHIP_FINISH_READY;
}
static const xrtownershipadapterv1 ownerAdapter={sizeof(ownerAdapter),ownerHold,ownerDrop,ownerClaim,ownerRestore,NULL,ownerClear,NULL};
static bool ownerReady(const void* raw){(void)raw;return true;}
static const xrtownershipfinishv2 ownerCompletion={{sizeof(ownerCompletion),&ownerAdapter,ownerFinish},XRT_OWNERSHIP_FINISH_V2_TAG,ownerDropChecked,ownerReady};
static bool resolve(xrtownershipref ref,void* user,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** prepare,const xrtownershipfinishv1** finish)
{
    (void)user;*adapter=NULL;*prepare=NULL;*finish=NULL;
    if(ref.Ops==&ownerOps){*adapter=&ownerAdapter;*finish=&ownerCompletion.V1;return true;}
    *adapter=xuiProxyResourceOwnershipAdapterV1(ref);if(!*adapter)return false;
    *prepare=xuiProxyResourceOwnershipPreparationV1(ref);
    const xrtownershipfinishv2* exact=xuiProxyResourceOwnershipCompletionV2(ref);assert(exact);
    *finish=&exact->V1;return true;
}
static int finalize(xui_proxy proxy,void* raw)
{
    Payload* payload=raw;assert(proxy && xuiProxyIsOwned(proxy));++payload->owner->payloads;
    if(payload->diagnostic){
        xerror* error=xrtErrorCreate(XERR_STATE,"payload",payload->status,"native payload");
        xrtSetErrorTake(error);
        if(!error && payload->status==XUI_OK)return XUI_ERROR_OUT_OF_MEMORY;
    }
    return payload->status;
}
static xui_proxy create(Owner* owner)
{
    xui_proxy_t table={0};table.iSize=sizeof(table);table.iVersion=XUI_PROXY_VERSION;table.pUser=owner;
    xui_callback_owner_t callback={{owner,&ownerOps},ownerHold,ownerDropChecked};xui_proxy proxy=NULL;
    assert(xuiProxyCreateOwned(&table,&callback,&proxy)==XUI_OK);return proxy;
}
static xui_font font(xui_proxy proxy,Owner* owner,int status,bool diagnostic)
{
    xui_font font=NULL;void* raw=NULL;assert(xuiFontCreateOwned(proxy,sizeof(Payload),finalize,&font,&raw)==XUI_OK);
    *(Payload*)raw=(Payload){owner,status,diagnostic};return font;
}
static void emptyMailbox(void)
{xl6_error_result result={0};uint64_t count=99;assert(xl6CallbackFailureTakeV1(&result,&count) && result.ok && !result.error && !count);}
static void snapshot(bool fail,bool diagnostic,bool scoped,bool oom,bool priorNull)
{
    Owner owner={.refs=1};xui_proxy proxy=create(&owner);xui_font value=font(proxy,&owner,fail?-8:0,diagnostic);
    XL6_OwnershipPlan* plan=NULL;assert(xl6OwnershipPlanTryCreateV3(xuiFontOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    bool consumed=false;assert(xuiProxyReleaseOwnerChecked(proxy,&consumed)==XUI_OK && consumed);
    assert(xuiFontReleaseOwnerChecked(value,&consumed)==XUI_OK && consumed && !owner.payloads);
    assert(ownerDropChecked(&owner));
    xl6_error_scope errors={0};if(scoped)assert(xl6ErrorScopeBegin(&errors));if(priorNull)xl6ErrorScopeCaptureFailure(&errors);
    if(oom)assert(xrtMemDebugFailAfter(0));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    bool triggered=oom && xrtMemDebugFailTriggered();if(oom)xrtMemDebugFailClear();
    assert(owner.refs==0 && owner.disposed==1 && owner.payloads==1);
    bool failed=fail||diagnostic||priorNull;
    if(scoped){
        xl6_error_result result=xl6ErrorScopeEnd(&errors);assert(result.ok==!failed);
        if(priorNull)assert(!result.error);
        else if(!failed)assert(!result.error);
        else if(!oom)assert(result.error && xrtErrorCode(result.error)==(fail?-8:0));
        xrtErrorFree(result.error);emptyMailbox();
    }else{
        xl6_error_result result={0};uint64_t count=99;
        assert(xl6CallbackFailureTakeV1(&result,&count));
        if(result.ok!=!failed || count!=(failed?1u:0u))fprintf(stderr,"snapshot failure delivery: fail=%d diagnostic=%d scoped=%d oom=%d expected=%d ok=%d count=%llu kind=%d code=%lld\n",fail,diagnostic,scoped,oom,failed,result.ok,(unsigned long long)count,(int)xrtErrorKind(result.error),(long long)xrtErrorCode(result.error));
        assert(result.ok==!failed && count==(failed?1u:0u));
        xrtErrorFree(result.error);
    }
    if(oom && failed)assert(triggered);
    xrtClearError();
}
static void cycle(void)
{
    Owner owner={.refs=1};xui_proxy proxy=create(&owner);owner.font=font(proxy,&owner,0,false);
    bool consumed=false;assert(xuiProxyReleaseOwnerChecked(proxy,&consumed)==XUI_OK && consumed);assert(ownerDropChecked(&owner));
    XL6_OwnershipPlan* plan=NULL;xrtownershipref root={&owner,&ownerOps};
    XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV3(root,resolve,NULL,&plan);
    assert(status==XL6_OWNERSHIP_PLAN_FINALIZERS || status==XL6_OWNERSHIP_PLAN_READY);
    if(status==XL6_OWNERSHIP_PLAN_FINALIZERS)assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(owner.payloads==1 && !owner.disposed);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(owner.refs==0 && owner.disposed==1 && owner.payloads==1 && !owner.font);emptyMailbox();
}
int main(void)
{
    assert(xrtMemDebugEnable(true));emptyMailbox();
    for(unsigned round=0;round<8;++round){
        for(unsigned bits=0;bits<16;++bits)snapshot((bits&1)!=0,(bits&2)!=0,(bits&4)!=0,(bits&8)!=0,false);
        snapshot(true,true,true,false,true);cycle();
    }
    puts("actual native Proxy/Font nodes through resident FinishV2 plan: shared cycle, committed cleanup, last snapshot failure, OOM, first NULL scope and mailbox: PASS");return 0;
}
