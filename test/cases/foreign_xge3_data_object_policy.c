/* Actual private policy TU + two standalone production DataObject C components.
 * This deliberately does not bind or claim a complete engine DLL/SDK. */
#include <xge3_runtime.h>
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_callback_failure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned holds,drops;
static bool dropFails;
static bool hold(const void* raw)
{assert(xl6ForeignViewRetainV1((XL6_ForeignView*)raw));++holds;return true;}
static bool drop(const void* raw)
{++drops;bool ok=xl6ForeignViewReleaseV1((XL6_ForeignView*)raw);return ok&&!dropFails;}
static XL6_ForeignView* wrap(void* raw,const XL6_ForeignViewDescriptorV1* policy)
{
    assert(policy && !policy->Watch && !policy->CallBegin && !policy->CallEnd && policy->TraceRaw && policy->Busy);
    assert(policy->flags==(XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE));
    XL6_ForeignView* view=xl6ForeignViewCreateRootWithDomainV1(raw,policy,&xl6Xge3TransferDomainV1,NULL);assert(view);return view;
}
static void closeView(XL6_ForeignView* view)
{assert(xl6ForeignViewCloseV1(view)&&xl6ForeignViewReleaseV1(view)&&xl6ForeignViewDrainV1());}
static size_t count(xrtownershipref ref)
{xrtownershipscope scope={0};size_t result=0;assert(xrtOwnershipFreezeTryBegin(&scope));assert(ref.Ops->Count(ref.Data,&result));assert(xrtOwnershipScopeEnd(&scope));return result;}
static bool poisonCount(const void* raw,size_t* result){(void)raw;(void)result;assert(!"unknown Ops must not be invoked");return false;}
static bool poisonTrace(const void* raw,xrtownershipvisitor v,void* user){(void)raw;(void)v;(void)user;assert(!"unknown Ops must not be invoked");return false;}
static const xrtownershipops poisonOps={poisonCount,poisonTrace};
static void domain(xrtownershipref ref)
{
    const XL6_OwnershipDomainV1* d=&xl6Xge3TransferDomainV1;assert(d->size==sizeof(XL6_OwnershipDomainV2)&&!d->Load);
    const XL6_OwnershipDomainV2* v2=(const XL6_OwnershipDomainV2*)d;assert(v2->abiTag==XL6_OWNERSHIP_DOMAIN_V2_TAG);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const xrtownershipfinishv1* finish=NULL;
    const xrtownershipfinalizationv1* finalization=NULL;xrtownershipscope freeze={0};
    assert(xrtMemDebugFailAfter(0));assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(d->Resolve(ref,NULL,&adapter,&preparation,&finish)&&adapter&&preparation&&finish&&d->AdmitsRead(ref));
    assert(preparation->Adapter==adapter&&finish->Adapter==adapter);
    assert(finish->size==sizeof(xrtownershipfinishv2));const xrtownershipfinishv2* f2=(const xrtownershipfinishv2*)finish;
    assert(f2->AbiTag==XRT_OWNERSHIP_FINISH_V2_TAG&&f2->DropReady&&f2->DropChecked);
    assert(v2->ResolveFinalization(ref,adapter,&finalization)&&finalization&&finalization->Adapter==adapter);
    assert(finalization->size==sizeof(*finalization)&&finalization->AbiTag==XRT_OWNERSHIP_FINALIZATION_V1_TAG);
    xrtownershipref unknown={ref.Data,&poisonOps};adapter=(const void*)1;preparation=(const void*)1;finish=(const void*)1;
    assert(!d->Resolve(unknown,NULL,&adapter,&preparation,&finish)&&!adapter&&!preparation&&!finish&&!d->AdmitsRead(unknown));
    finalization=(const void*)1;assert(v2->ResolveFinalization(unknown,NULL,&finalization)&&!finalization);
    assert(xrtOwnershipScopeEnd(&freeze)&&!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
}

#define FAMILY(Name,Prefix,Handle,Output,Owner,Policy,OK,CALLBACK_FAILED) \
typedef struct Name##State {XL6_ForeignView* view;bool closeInside;int callbackStatus;unsigned calls;} Name##State; \
static int Name##Read(const xstrview* format,Output* output,void* raw) \
{Name##State* s=raw;assert(format->Size==1&&format->Data[0]=='p');++s->calls; \
 if(s->closeInside){assert(xl6ForeignViewCloseV1(s->view));assert(xl6ForeignViewPendingV1(0));} \
 return s->callbackStatus ? s->callbackStatus:Prefix##OutputSet(output,"a\0b",3);} \
static void Name##Basic(void) \
{Handle object=NULL,shared=NULL;assert(Prefix##ObjectCreateOwned(&object)==OK);domain(Prefix##ObjectOwnership(object)); \
 XL6_ForeignView* view=wrap(object,&Policy);assert(Prefix##ObjectShareOwned(object,&shared)==OK&&shared==object); \
 assert(count(Prefix##ObjectOwnership(object))==2);XL6_ForeignViewCallV1 call={0};assert(xl6ForeignViewCallBeginV1(view,&call,NULL,0)); \
 const char text[3]={'a',0,'b'};char output[3]={0};size_t size=0;assert(Prefix##ObjectSet(object,"v",text,3)==OK); \
 assert(Prefix##ObjectGet(object,"v",output,sizeof(output),&size)==OK&&size==3&&!memcmp(output,text,3)); \
 assert(xl6ForeignViewCallEndV1(&call));closeView(view);assert(count(Prefix##ObjectOwnership(shared))==1);assert(Prefix##ObjectReleaseChecked(shared)==OK);} \
static void Name##Active(void) \
{Handle object=NULL;assert(Prefix##ObjectCreateOwned(&object)==OK);Name##State state={wrap(object,&Policy),true,0,0}; \
 Owner owner={xl6ForeignViewOwnershipV1(state.view),hold,drop};assert(Prefix##ObjectSetProviderOwned(object,"p",Name##Read,&state,&owner)==OK); \
 XL6_ForeignViewCallV1 call={0};assert(xl6ForeignViewCallBeginV1(state.view,&call,NULL,0));char data[3]={0};size_t size=0; \
 assert(Prefix##ObjectGet(object,"p",data,3,&size)==OK&&state.calls==1&&size==3&&!memcmp(data,"a\0b",3)); \
 assert(xl6ForeignViewPendingV1(0)&&!xrtGetError());assert(xl6ForeignViewCallEndV1(&call)); \
 assert(xl6ForeignViewReleaseV1(state.view)&&xl6ForeignViewDrainV1()&&!xl6ForeignViewPendingV1(0));assert(holds==drops);} \
static void Name##CallbackStatus(void) \
{Handle object=NULL;assert(Prefix##ObjectCreateOwned(&object)==OK);Name##State state={wrap(object,&Policy),false,1,0}; \
 Owner owner={xl6ForeignViewOwnershipV1(state.view),hold,drop};assert(Prefix##ObjectSetProviderOwned(object,"p",Name##Read,&state,&owner)==OK); \
 XL6_ForeignViewCallV1 call={0};assert(xl6ForeignViewCallBeginV1(state.view,&call,NULL,0));char data[3];size_t size=0; \
 assert(Prefix##ObjectGet(object,"p",data,3,&size)==CALLBACK_FAILED&&state.calls==1&&!xrtGetError()); \
 assert(xl6ForeignViewCallEndV1(&call));closeView(state.view);assert(holds==drops);} \
static void Name##ConsumedFailure(void) \
{Handle object=NULL;assert(Prefix##ObjectCreateOwned(&object)==OK);Name##State state={wrap(object,&Policy),false,0,0}; \
 Owner owner={xl6ForeignViewOwnershipV1(state.view),hold,drop};assert(Prefix##ObjectSetProviderOwned(object,"p",Name##Read,&state,&owner)==OK); \
 unsigned before=drops;dropFails=true;xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors)&&xrtMemDebugFailAfter(0)); \
 bool ok=xl6ForeignViewCloseV1(state.view);if(!ok)xl6ErrorScopeCaptureFailure(&errors);xl6_error_result result=xl6ErrorScopeEnd(&errors); \
 assert(!ok&&!result.ok&&xrtMemDebugFailTriggered()&&drops==before+1);xrtErrorFree(result.error);xrtMemDebugFailClear();dropFails=false; \
 closeView(state.view);assert(drops==before+1&&holds==drops);}
FAMILY(Engine,xgeData,xge_data_object,xge_data_output_t,xge_callback_owner_t,xl6Xge3DataObjectPolicy,XGE_OK,XGE_ERROR_CALLBACK_FAILED)
FAMILY(Ui,xuiData,xui_data_object,xui_data_output_t,xui_callback_owner_t,xl6Xui3DataObjectPolicy,XUI_OK,XUI_ERROR_CALLBACK_FAILED)
#undef FAMILY
static void cycle(void)
{
    xge_data_object engine=NULL;xui_data_object ui=NULL;assert(xgeDataObjectCreateOwned(&engine)==XGE_OK&&xuiDataObjectCreateOwned(&ui)==XUI_OK);
    XL6_ForeignView* a=wrap(engine,&xl6Xge3DataObjectPolicy);XL6_ForeignView* b=wrap(ui,&xl6Xui3DataObjectPolicy);
    EngineState sa={a,false,0,0};UiState sb={b,false,0,0};xge_callback_owner_t ab={xl6ForeignViewOwnershipV1(b),hold,drop};
    xui_callback_owner_t ba={xl6ForeignViewOwnershipV1(a),hold,drop};
    assert(xgeDataObjectSetProviderOwned(engine,"p",EngineRead,&sa,&ab)==XGE_OK&&xuiDataObjectSetProviderOwned(ui,"p",UiRead,&sb,&ba)==XUI_OK);
    assert(xl6ForeignViewReleaseV1(a)&&xl6ForeignViewReleaseV1(b));
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get()};XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
    XL6_OwnershipPlan* plan=NULL;XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreateV8(xl6ForeignViewOwnershipV1(a),xl6OwnershipCatalogProbeV1,NULL,
        xl6OwnershipCatalogResolveFinalizationV1,false,catalog,xl6OwnershipCatalogBoundaryV1Get(catalog),&plan);
    assert(result==XL6_OWNERSHIP_PLAN_PREPARATIONS||result==XL6_OWNERSHIP_PLAN_FINALIZERS);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);bool failed=false;
    assert(xl6OwnershipPlanFinalizeStepV1(plan,&failed)==XL6_OWNERSHIP_STEP_READY&&!failed);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY&&xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    xl6OwnershipCatalogDestroyV1(catalog);assert(xl6ForeignViewDrainV1()&&holds==drops&&!xl6ForeignViewPendingV1(0));
}
int main(void)
{
    assert(XGE_BUILD_ABI_FINGERPRINT==0&&xrtMemDebugEnable(true));
    EngineBasic();UiBasic();EngineActive();UiActive();EngineCallbackStatus();UiCallbackStatus();cycle();EngineConsumedFailure();UiConsumedFailure();
    assert(holds==drops&&!xrtGetError()&&!xl6ForeignViewPendingV1(0));
    puts("Actual DataObject private policies: two families, active-close wait, real cross-family FV cycle, leased FinalizationV1, consumed nullable failure: PASS");return 0;
}
