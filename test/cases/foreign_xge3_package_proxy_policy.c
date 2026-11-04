/* Native resource policies and cold domain through the real ForeignView graph.
 * C callback state is intentionally resident in this executable; script/module
 * code residency is qualified separately by the end-to-end package tests. */
#include <xge3_runtime.h>
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_native_callback.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_callback_failure.h"
#include <assert.h>
#include <stdio.h>

typedef struct Payload {unsigned* disposed;bool fail;} Payload;
static int finalize(xui_proxy proxy,void* raw)
{Payload* payload=raw;assert(xuiProxyIsOwned(proxy));++*payload->disposed;return payload->fail ? -7:XUI_OK;}
static XL6_ForeignView* createFont(xui_proxy proxy,unsigned* disposed,bool fail,xui_font* output)
{
    void* payload=NULL;assert(xuiFontCreateOwned(proxy,sizeof(Payload),finalize,output,&payload)==XUI_OK);
    *(Payload*)payload=(Payload){disposed,fail};
    XL6_ForeignView* view=xl6ForeignViewCreateRootWithDomainV1(*output,&xl6Xui3FontPolicy,&xl6Xui3OwnershipDomainV1,NULL);
    assert(view);return view;
}
static void shared(void)
{
    unsigned disposed=0;xui_proxy proxy=NULL;assert(xuiProxyXgeCreate(&proxy)==XUI_OK);
    XL6_ForeignView* proxyView=xl6ForeignViewCreateRootWithDomainV1(proxy,&xl6Xui3ProxyPolicy,&xl6Xui3OwnershipDomainV1,NULL);assert(proxyView);
    xui_font font=NULL;XL6_ForeignView* view=createFont(proxy,&disposed,false,&font);
    assert(xuiFontAddRef(font)==XUI_OK);
    assert(xl6ForeignViewCloseV1(proxyView) && xl6ForeignViewReleaseV1(proxyView));
    assert(xuiFontGetProxy(font)==proxy && !disposed);
    XL6_ForeignViewCallV1 call={0};assert(xl6ForeignViewCallBeginV1(view,&call,NULL,0));
    assert(xl6ForeignViewCloseV1(view) && !disposed);assert(xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewReleaseV1(view) && !disposed);
    assert(xuiFontReleaseChecked(font)==XUI_OK && disposed==1);
}
static void failed(bool oom)
{
    unsigned disposed=0;xui_proxy proxy=NULL;assert(xuiProxyXgeCreate(&proxy)==XUI_OK);
    xui_font font=NULL;XL6_ForeignView* view=createFont(proxy,&disposed,true,&font);
    assert(xuiProxyReleaseChecked(proxy)==XUI_OK);
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    if(oom)assert(xrtMemDebugFailAfter(0));
    assert(!xl6ForeignViewCloseV1(view));
    if(oom){assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();}
    xl6_error_result result=xl6ErrorScopeEnd(&scope);assert(!result.ok);
    if(!oom)assert(result.error && xrtErrorCode(result.error)==-7);
    xrtErrorFree(result.error);assert(disposed==1);
    assert(xl6ForeignViewCloseV1(view) && xl6ForeignViewReleaseV1(view));
}
static void protectedWidget(void)
{
#if XUI_ENABLE_WINDOW
    xui_context context=NULL;xui_widget window=NULL;
    assert(xuiCreate(&context)==XUI_OK && xuiWindowCreate(context,&window,NULL)==XUI_OK);
    xui_widget client=xuiWindowGetClientWidget(window);assert(client && xuiWidgetGetContext(client)==context);
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(xrtMemDebugFailAfter(0));
    assert(!xl6Xui3WidgetPolicy.Destroy(client));
    assert(xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    xl6_error_result failure=xl6ErrorScopeEnd(&scope);
    assert(!failure.ok && (!failure.error || xrtErrorKind(failure.error)==XERR_MEMORY));xrtErrorFree(failure.error);
    assert(xuiWidgetGetContext(client)==context);xuiDestroy(context);
#endif
}
typedef struct Capture {XL6_ForeignView* view;unsigned* dropped;} Capture;
static const int captureIdentity=1;
static bool captureValid(const void* layout,const void* raw,size_t bytes)
{return layout==&captureIdentity && raw && bytes==sizeof(Capture);}
static bool captureTrace(const void* layout,const void* raw,size_t bytes,XL6_ModuleOwnershipVisit visit,void* user)
{assert(captureValid(layout,raw,bytes));const Capture* capture=raw;return !capture->view || visit(xl6ForeignViewOwnershipV1(capture->view),user);}
static bool captureDrop(const void* layout,void* raw,size_t bytes)
{
    assert(captureValid(layout,raw,bytes));Capture* capture=raw;XL6_ForeignView* view=capture->view;
    ++*capture->dropped;xrtFree(capture);return !view || xl6ForeignViewReleaseV1(view);
}
static const XL6_ClosureEnvironmentOpsV1 captureOps={sizeof(captureOps),captureValid,captureTrace,captureDrop};
static void invoke(void){}
static void cycle(void)
{
    unsigned disposed=0,dropped=0;Capture* capture=xrtCalloc(1,sizeof(*capture));assert(capture);capture->dropped=&dropped;
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(NULL,invoke,NULL,capture,sizeof(*capture),&captureIdentity,&captureOps,NULL,NULL,0);assert(closure);
    const XL6_NativeCallbackRuntimeV1* callbacks=xl6NativeCallbackRuntimeV1Get();
    XL6_NativeCallback* callback=callbacks->Create(NULL,closure,NULL,9);assert(callback);
    xui_callback_owner_t owner={callbacks->Ownership(callback),callbacks->HoldNative,callbacks->DropNative};
    xui_proxy_t table={0};table.iSize=sizeof(table);table.iVersion=XUI_PROXY_VERSION;
    xui_proxy proxy=NULL;assert(xuiProxyCreateOwned(&table,&owner,&proxy)==XUI_OK);
    xui_font font=NULL;XL6_ForeignView* view=createFont(proxy,&disposed,false,&font);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));capture->view=xl6ForeignViewRetainV1(view);assert(capture->view);assert(xrtOwnershipScopeEnd(&mutation));
    assert(callbacks->Release(callback));xl6ClosureRuntimeV1Get()->Release(closure);
    assert(xuiProxyReleaseChecked(proxy)==XUI_OK && xl6ForeignViewReleaseV1(view));
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),xl6OwnershipDomainClosureV1Get(),xl6OwnershipDomainNativeCallbackV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);
    const XL6_OwnershipBoundaryV1* boundary=xl6OwnershipCatalogBoundaryV1Get(catalog);assert(boundary);
    XL6_OwnershipPlan* plan=NULL;
    XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV7(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,NULL,false,catalog,boundary,&plan);
    assert(status==XL6_OWNERSHIP_PLAN_FINALIZERS || status==XL6_OWNERSHIP_PLAN_PREPARATIONS);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(disposed==1 && dropped==1);xl6OwnershipCatalogDestroyV1(catalog);
    assert(!xl6ForeignViewPendingV1(0));
}
int main(void)
{
    assert(!xl6Xge3RuntimeBindV1());assert(xrtMemDebugEnable(true));shared();failed(false);failed(true);protectedWidget();cycle();
    uint64_t pending,total;assert(xl6CallbackFailureCountV1(&pending,&total) && !pending);
    puts("actual Proxy/Font policies: shared native owner, consumed failed close, aggregate FinishV2 domain and callback/view cycle: PASS");return 0;
}
