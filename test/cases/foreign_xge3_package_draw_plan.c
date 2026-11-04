/* Real native Draw/Surface/Proxy nodes through the resident planner. CPU only. */
#define XRT_MODULE_MEMORY_DEBUG
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/api/xl_foreign_error.h"
#include <xui.h>
#include <assert.h>
#include <stdio.h>
typedef struct Owner {size_t refs;unsigned ended,targetFreed,dropped,sequence,endOrder,targetOrder;xui_draw_context draw;const void* claim;bool cleared,finished;int failure;} Owner;
static bool count(const void* raw,size_t* out){*out=((const Owner*)raw)->refs;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user){const Owner* o=raw;return o->cleared || !o->draw || visit(xuiDrawOwnership(o->draw),user);}
static const xrtownershipops ops={count,trace};
static bool hold(const void* raw){Owner* o=(void*)raw;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));++o->refs;assert(xrtOwnershipScopeEnd(&s));return true;}
static bool dropChecked(const void* raw){Owner* o=(void*)raw;xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));assert(o->refs);--o->refs;++o->dropped;assert(xrtOwnershipScopeEnd(&s));return true;}
static void drop(const void* raw){assert(dropChecked(raw));}
static bool ready(const void* raw){(void)raw;return true;}
static bool claim(const void* raw,const void* token){Owner* o=(void*)raw;if(o->claim && o->claim!=token)return false;o->claim=token;return true;}
static void restore(const void* raw,const void* token){Owner* o=(void*)raw;assert(o->claim==token && !o->cleared);o->claim=NULL;}
static void clear(const void* raw,const void* token){Owner* o=(void*)raw;assert(o->claim==token);o->cleared=true;}
static xrtownershipfinishresult finish(const void* raw,const void* token)
{
    Owner* o=(void*)raw;assert(o->claim==token && o->cleared);if(o->finished)return XRT_OWNERSHIP_FINISH_READY;
    xrtownershipscope s={0};assert(xrtOwnershipMutationBegin(&s));xui_draw_context draw=o->draw;o->draw=NULL;o->finished=true;assert(xrtOwnershipScopeEnd(&s));
    if(draw){bool consumed=false;assert(xuiDrawReleaseOwnerChecked(draw,&consumed)==XUI_OK && consumed);}
    return XRT_OWNERSHIP_FINISH_READY;
}
static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,NULL};
static const xrtownershipfinishv2 completion={{sizeof(completion),&adapter,finish},XRT_OWNERSHIP_FINISH_V2_TAG,dropChecked,ready};
static bool resolve(xrtownershipref ref,void* user,const xrtownershipadapterv1** a,const xrtownershippreparationv1** p,const xrtownershipfinishv1** f)
{
    (void)user;*a=NULL;*p=NULL;*f=NULL;
    if(ref.Ops==&ops){*a=&adapter;*f=&completion.V1;return true;}
    *a=xuiProxyResourceOwnershipAdapterV1(ref);if(!*a)return false;
    *p=xuiProxyResourceOwnershipPreparationV1(ref);*f=&xuiProxyResourceOwnershipCompletionV2(ref)->V1;return true;
}
static int end(xui_proxy proxy,void* raw)
{
    Owner* o=*(Owner**)raw;assert(proxy && !o->targetFreed && !o->ended);o->ended=1;o->endOrder=++o->sequence;return o->failure;
}
static int freeTarget(xui_proxy proxy,void* raw)
{
    Owner* o=*(Owner**)raw;assert(proxy && o->ended==1 && !o->targetFreed);o->targetFreed=1;o->targetOrder=++o->sequence;return XUI_OK;
}
static void cycle(bool targetFirst,bool failure)
{
    Owner owner={.refs=1,.failure=failure?-27:0};xui_proxy_t table={0};table.iSize=sizeof(table);table.iVersion=XUI_PROXY_VERSION;
    xui_callback_owner_t backend={{&owner,&ops},hold,dropChecked};xui_proxy proxy=NULL;assert(xuiProxyCreateOwned(&table,&backend,&proxy)==XUI_OK);
    xui_surface target=NULL;void* data=NULL;assert(xuiSurfaceCreateOwned(proxy,sizeof(Owner*),freeTarget,&target,&data)==XUI_OK);*(Owner**)data=&owner;
    assert(xuiDrawCreateOwned(proxy,target,sizeof(Owner*),end,&owner.draw,&data)==XUI_OK);*(Owner**)data=&owner;
    xrtownershipref root=targetFirst?xuiSurfaceOwnership(target):xuiDrawOwnership(owner.draw);
    assert(xuiSurfaceReleaseChecked(target)==XUI_OK && xuiProxyReleaseChecked(proxy)==XUI_OK && dropChecked(&owner));
    XL6_OwnershipPlan* plan=NULL;assert(xl6OwnershipPlanTryCreateV3(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
    assert(!owner.ended && !owner.targetFreed);
    XL6_OwnershipStepResult prepared=xl6OwnershipPlanPrepare(plan);
    assert(prepared==(failure?XL6_OWNERSHIP_STEP_FAILED:XL6_OWNERSHIP_STEP_READY));
    if(failure){assert(!xrtGetError() || xrtErrorCode(xrtGetError())==-27);xrtClearError();assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);}
    assert(owner.ended==1 && !owner.targetFreed);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(owner.targetFreed==1 && owner.endOrder<owner.targetOrder);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(!owner.refs && !owner.draw && owner.ended==1 && owner.targetFreed==1 && !xrtGetError());
}
int main(void)
{
    for(unsigned i=0;i<16;++i){cycle(false,false);cycle(true,false);cycle(false,true);cycle(true,true);}
    puts("Native Draw preparation precedes target Finalize in actual plan; terminal header, reverse discovery and failing End consumed once: PASS");return 0;
}
