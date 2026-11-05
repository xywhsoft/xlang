#define XRT_MODULE_MEMORY_DEBUG
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_foreign_view.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_static_module.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Link{void(*notify)(void*);void* user;void** slot;} Link;
typedef struct Raw{Link* watch;Link* childWatch;XL6_ForeignView *capture,*child;unsigned calls,waits;bool alive,childAlive,readBusy,makeBusy,fail,failIncomplete,failedOnce;} Raw;
static bool watch(void* data,void* storage,void(*notify)(void*),void* user)
{Raw* r=data;Link* link=storage;assert(r->alive&&!r->watch);*link=(Link){notify,user,(void**)&r->watch};r->watch=link;return true;}
static bool childWatch(void* data,void* storage,void(*notify)(void*),void* user)
{Raw* r=data;Link* link=storage;assert(r->alive&&r->childAlive&&!r->childWatch);*link=(Link){notify,user,(void**)&r->childWatch};r->childWatch=link;return true;}
static void unwatch(void* storage)
{Link* link=storage;if(link->slot&&*link->slot==link)*link->slot=NULL;memset(link,0,sizeof(*link));}
static void invalidate(Link* link)
{if(!link)return;void(*notify)(void*)=link->notify;void* user=link->user;*link->slot=NULL;memset(link,0,sizeof(*link));notify(user);}
static bool begin(void* data,void* storage)
{Raw* r=data;(void)storage;return r->alive;}
static bool end(void* storage){(void)storage;return true;}
static bool busy(const void* data){return ((const Raw*)data)->readBusy;}
static bool trace(const void* data,XL6_ModuleOwnershipVisit visit,void* user)
{const Raw* r=data;return (!r->capture||visit(xl6ForeignViewOwnershipV1(r->capture),user))&&(!r->child||visit(xl6ForeignViewOwnershipV1(r->child),user));}
static bool destroy(void* data)
{
 Raw* r=data;assert(r->alive);++r->calls;
 if(r->waits){--r->waits;if(r->makeBusy)r->readBusy=true;return false;}
 if(r->failIncomplete&&!r->failedOnce){r->failedOnce=true;xl6_error_result failure={false,NULL};assert(xl6ErrorScopeTryPropagateFailureV1(&failure));return false;}
 r->readBusy=false;r->alive=r->childAlive=false;
 invalidate(r->childWatch);invalidate(r->watch);
 XL6_ForeignView *a=r->capture,*b=r->child;r->capture=r->child=NULL;
 if(a)assert(xl6ForeignViewReleaseV1(a));
 if(b)assert(xl6ForeignViewReleaseV1(b));
 if(r->fail){xl6_error_result failure={false,NULL};assert(xl6ErrorScopeTryPropagateFailureV1(&failure));}
 return true;
}
static bool destroy_child(void* data){(void)data;assert(!"borrowed child must not destroy native payload");return false;}
static const XL6_ForeignViewDescriptorV1 rootDescriptor={sizeof(rootDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"fake.context",
 sizeof(Link),_Alignof(Link),watch,unwatch,destroy,1,1,begin,end,trace,busy,
 XL6_FOREIGN_VIEW_RELIABLE_OWNED_WATCH|XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY|XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE|
 XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,NULL};
static const XL6_ForeignViewDescriptorV1 childDescriptor={sizeof(childDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"fake.child",
 sizeof(Link),_Alignof(Link),childWatch,unwatch,destroy_child,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static XL6_ForeignView* create(Raw* r,bool capture,bool child)
{
 r->alive=true;XL6_ForeignView* view=xl6ForeignViewCreateRootV1(r,&rootDescriptor,NULL);assert(view);
 if(capture)r->capture=xl6ForeignViewRetainV1(view);
 if(child){XL6_ForeignViewCallV1 call={0};char native=0;r->childAlive=true;
  assert(xl6ForeignViewCallBeginV1(view,&call,&native,sizeof(native)));
  XL6_ForeignView* value=xl6ForeignViewBorrowChildV1(r,view,&childDescriptor,NULL);assert(value);
  r->child=xl6ForeignViewRetainV1(value);assert(r->child);
  assert(xl6ForeignViewReleaseV1(value));assert(xl6ForeignViewCallEndV1(&call));
 }
 return view;
}
static void run(bool fail,bool incomplete,bool child,bool knownBusy)
{
 Raw other={.waits=20},raw={.waits=knownBusy?1:0,.makeBusy=knownBusy,.fail=fail,.failIncomplete=incomplete};
 XL6_ForeignView* peer=create(&other,false,false);assert(xl6ForeignViewCloseV1(peer)&&other.alive&&other.calls==1);
 XL6_ForeignView* view=create(&raw,true,child);assert(xl6ForeignViewReleaseV1(view));other.calls=0;
 const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get()};
 XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
 XL6_OwnershipPlan* plan=NULL;
 assert(xl6OwnershipPlanTryCreateV8(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,NULL,
  xl6OwnershipCatalogResolveFinalizationV1,false,catalog,xl6OwnershipCatalogBoundaryV1Get(catalog),&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
 assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);bool failed=false;
 XL6_OwnershipStepResult result=xl6OwnershipPlanFinalizeStepV1(plan,&failed);
 if(knownBusy){assert(result==XL6_OWNERSHIP_STEP_WAITING&&!failed&&raw.alive&&raw.calls==1&&!other.calls);
  result=xl6OwnershipPlanFinalizeStepV1(plan,&failed);}
 if(incomplete){assert(result==XL6_OWNERSHIP_STEP_WAITING&&failed&&raw.alive&&!xrtGetError());
  unsigned calls=raw.calls;assert(xl6OwnershipPlanFinalizeStepV1(plan,&failed)==XL6_OWNERSHIP_STEP_WAITING&&!failed&&raw.calls==calls);
  assert(xl6OwnershipPlanRetryFinalizationV1(plan));result=xl6OwnershipPlanFinalizeStepV1(plan,&failed);
 }
 if(fail){assert(failed&&!raw.alive&&!xrtGetError());result=xl6OwnershipPlanFinalizeStepV1(plan,&failed);assert(!failed);}
 assert(result==XL6_OWNERSHIP_STEP_READY&&!failed&&!raw.alive&&!raw.capture&&!raw.child&&!raw.watch&&!raw.childWatch&&!other.calls);
 assert(!xrtGetError());assert(xl6ForeignViewPendingV1(0)==1); /* unrelated owner only */
 assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
 assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
 assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);assert(!other.calls);
 xl6OwnershipCatalogDestroyV1(catalog);other.waits=0;
 assert(xl6ForeignViewCloseV1(peer)&&!other.alive&&other.calls==1);
 assert(xl6ForeignViewReleaseV1(peer));assert(!xl6ForeignViewPendingV1(0));
}
static XL6_ForeignView* staticGlobal;
static unsigned staticFinished,staticFinishWait;
static bool staticFinishFail;
static bool staticTrace(XL6_ModuleOwnershipVisit visit,void* user)
{return !staticGlobal||visit(xl6ForeignViewOwnershipV1(staticGlobal),user);}
static int staticFinish(void)
{assert(staticGlobal);++staticFinished;assert(xl6ForeignViewReleaseV1(staticGlobal));staticGlobal=NULL;return 1;}
static XL6_ModuleRetireResult staticFinishStep(void)
{if(staticFinishWait){--staticFinishWait;return XL6_MODULE_RETIRE_BUSY;}
 if(staticFinishFail){staticFinishFail=false;xrtSetErrorKind(XERR_ARGUMENT);return XL6_MODULE_RETIRE_FAILED;}
 return staticFinish()?XL6_MODULE_RETIRE_READY:XL6_MODULE_RETIRE_FAILED;}
static void staticRun(unsigned mode)
{
 Raw raw={.alive=true,.waits=mode==0?2:0,.makeBusy=true,.fail=mode==1,.failIncomplete=mode==2};
 staticFinished=staticFinishWait=0;staticFinishFail=false;
 if(mode==3)staticFinishWait=2;
 if(mode==1){staticFinishWait=1;staticFinishFail=true;}
 const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),xl6OwnershipDomainCodeLeaseV1Get()};
 XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
 static const XL6_ModuleRetirementV1 graph={sizeof(graph),UINT64_C(0x584C364D525431),staticTrace,staticFinish};
 XL6_StaticModuleContractV1 base={sizeof(base),&graph,NULL,staticFinishStep,xl6OwnershipCatalogResolveV1,catalog,NULL,NULL,NULL};
 XL6_StaticModuleContractV2 checked={sizeof(checked),&base,xl6OwnershipCatalogProbeV1};
 XL6_StaticModule* module=xl6StaticModuleCreateV4(&checked,xl6OwnershipCatalogBoundaryV1Get(catalog),xl6OwnershipCatalogResolveFinalizationV1);assert(module);
 staticGlobal=xl6ForeignViewCreateRootV1(&raw,&rootDescriptor,xl6StaticModuleOwnerV1(module));assert(staticGlobal);
 raw.capture=xl6ForeignViewRetainV1(staticGlobal);assert(raw.capture);
 XL6_StaticModuleResultV1 result=xl6StaticModuleRetireV1(module);
 for(unsigned i=0;i<4&&!raw.calls&&!result.ready;++i)result=xl6StaticModuleRetireV1(module);
 bool failed=!result.ok;
 if(mode==0)assert(!result.ready&&result.ok&&raw.calls==1&&raw.alive);
 if(mode==1||mode==2)assert(!result.ok&&!xrtGetError());
 xrtClearError();
 for(unsigned i=0;i<20&&!result.ready;++i){
  result=xl6StaticModuleDrainForV1(module,0);failed|=!result.ok;
  if(mode==1||mode==2)assert(!result.ok&&!xrtGetError());else assert(result.ok&&!xrtGetError());
 }
 assert(result.ready&&failed==(mode==1||mode==2)&&staticFinished==1&&!staticGlobal&&!raw.alive);
 assert(xl6StaticModuleDestroyV1(&module)&&!module);xl6OwnershipCatalogDestroyV1(catalog);xrtClearError();
}
int main(void)
{
 assert(xrtMemDebugEnable(true));
 run(false,false,true,false);staticRun(0);xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
 for(unsigned i=0;i<20;++i){run(false,false,true,true);run(true,false,true,false);run(false,true,true,false);run(false,false,false,false);}
 for(unsigned mode=0;mode<4;++mode)staticRun(mode);
 xrtClearError();xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
 puts("ForeignView semantic continuation: 81 real native owners, child invalidation/detach, independent Context queue untouched, known-read BUSY continuation, consumed-failure NULL and explicit incomplete retry; zero live delta");return 0;
}
