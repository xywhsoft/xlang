/* Real private Draw policy/domain against a matched native DLL; no device. */
#include <xge3_runtime.h>
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_native_callback.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_callback_failure.h"
#include <assert.h>
#include <stdio.h>
typedef struct State {unsigned ended,targetFreed;bool fail;} State;
static int end(xui_proxy proxy,void* raw){State* s=*(State**)raw;assert(proxy && !s->ended && !s->targetFreed);++s->ended;return s->fail?-23:XUI_OK;}
static int targetEnd(xui_proxy proxy,void* raw){State* s=*(State**)raw;assert(proxy && s->ended==1 && !s->targetFreed);++s->targetFreed;return XUI_OK;}
static XL6_ForeignView* createDraw(xui_proxy proxy,State* state,xui_draw_context* output)
{
    xui_surface target=NULL;void* payload=NULL;
    assert(xuiSurfaceCreateOwned(proxy,sizeof(State*),targetEnd,&target,&payload)==XUI_OK);*(State**)payload=state;
    assert(xuiDrawCreateOwned(proxy,target,sizeof(State*),end,output,&payload)==XUI_OK);*(State**)payload=state;
    assert(xuiSurfaceReleaseChecked(target)==XUI_OK);
    XL6_ForeignView* view=xl6ForeignViewCreateRootWithDomainV1(*output,&xl6Xui3DrawPolicy,&xl6Xui3OwnershipDomainV1,NULL);assert(view);return view;
}
static void scope(bool fail,bool closeInside,bool oom,bool priorNull)
{
    State state={.fail=fail};xui_proxy proxy=NULL;assert(xuiProxyXgeCreate(&proxy)==XUI_OK);
    xui_draw_context raw=NULL;XL6_ForeignView* view=createDraw(proxy,&state,&raw);assert(xuiProxyReleaseChecked(proxy)==XUI_OK);
    XL6_ForeignViewCallV1 call={0};xui_draw_scope_t nativeScope={0};
    assert(xl6ForeignViewCallBeginV1(view,&call,&nativeScope,sizeof(nativeScope)));
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));if(priorNull)xl6ErrorScopeCaptureFailure(&errors);
    if(oom)assert(xrtMemDebugFailAfter(0));
    if(closeInside)assert(xl6ForeignViewCloseV1(view));else assert(xuiDrawEndChecked(raw)==XUI_OK);
    assert(!state.ended && !state.targetFreed);
    assert(xl6ForeignViewCallEndV1(&call)==!fail);
    if(oom)xrtMemDebugFailClear();
    xl6_error_result result=xl6ErrorScopeEnd(&errors);assert(result.ok==!(fail||priorNull));
    if(priorNull)assert(!result.error);else if(fail && !oom)assert(result.error && xrtErrorCode(result.error)==-23);
    xrtErrorFree(result.error);assert(state.ended==1 && state.targetFreed==1);
    assert(xl6ForeignViewCloseV1(view) && xl6ForeignViewReleaseV1(view));
}
typedef struct Capture {XL6_ForeignView* view;unsigned* dropped;} Capture;
static const int identity=1;
static bool valid(const void* layout,const void* raw,size_t bytes){return layout==&identity && raw && bytes==sizeof(Capture);}
static bool trace(const void* layout,const void* raw,size_t bytes,XL6_ModuleOwnershipVisit visit,void* user)
{assert(valid(layout,raw,bytes));const Capture* c=raw;return !c->view || visit(xl6ForeignViewOwnershipV1(c->view),user);}
static bool drop(const void* layout,void* raw,size_t bytes)
{assert(valid(layout,raw,bytes));Capture* c=raw;XL6_ForeignView* view=c->view;++*c->dropped;xrtFree(c);return !view || xl6ForeignViewReleaseV1(view);}
static const XL6_ClosureEnvironmentOpsV1 captureOps={sizeof(captureOps),valid,trace,drop};
static void invoke(void){}
static void cycle(void)
{
    State state={0};unsigned dropped=0;Capture* capture=xrtCalloc(1,sizeof(*capture));assert(capture);capture->dropped=&dropped;
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(NULL,invoke,NULL,capture,sizeof(*capture),&identity,&captureOps,NULL,NULL,0);assert(closure);
    const XL6_NativeCallbackRuntimeV1* callbacks=xl6NativeCallbackRuntimeV1Get();XL6_NativeCallback* cb=callbacks->Create(NULL,closure,NULL,9);assert(cb);
    xui_callback_owner_t owner={callbacks->Ownership(cb),callbacks->HoldNative,callbacks->DropNative};
    xui_proxy_t table={0};table.iSize=sizeof(table);table.iVersion=XUI_PROXY_VERSION;xui_proxy proxy=NULL;assert(xuiProxyCreateOwned(&table,&owner,&proxy)==XUI_OK);
    xui_draw_context raw=NULL;XL6_ForeignView* view=createDraw(proxy,&state,&raw);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));capture->view=xl6ForeignViewRetainV1(view);assert(capture->view);assert(xrtOwnershipScopeEnd(&mutation));
    assert(callbacks->Release(cb));xl6ClosureRuntimeV1Get()->Release(closure);assert(xuiProxyReleaseChecked(proxy)==XUI_OK && xl6ForeignViewReleaseV1(view));
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),xl6OwnershipDomainClosureV1Get(),xl6OwnershipDomainNativeCallbackV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV7(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,NULL,false,catalog,xl6OwnershipCatalogBoundaryV1Get(catalog),&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY && state.ended==1);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY && state.targetFreed==1);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && dropped==1);xl6OwnershipCatalogDestroyV1(catalog);
}
int main(void)
{
    assert(!xl6Xge3RuntimeBindV1());assert(xrtMemDebugEnable(true));
    for(unsigned bits=0;bits<16;++bits)scope((bits&1)!=0,(bits&2)!=0,(bits&4)!=0,(bits&8)!=0);
    cycle();assert(!xl6ForeignViewPendingV1(0));uint64_t pending,total;assert(xl6CallbackFailureCountV1(&pending,&total) && !pending);
    puts("Draw private policy: Native Scope tail, consumed End failure, aggregate domain and actual closure/callback cycle: PASS");return 0;
}
