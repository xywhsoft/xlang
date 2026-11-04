/* Regression reproducer: a rooted native snapshot outlives the last creator
 * thread owner. No window/device/GPU is initialized. This intentionally
 * records the missing guard rather than claiming production acceptance. */
#include <xge3_runtime.h>
#include "src/runtime/xl6_native_callback.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Observation {uint64_t creator,dropThread;unsigned dropped;} Observation;
static const int identity=1;
static bool valid(const void* layout,const void* raw,size_t bytes)
{return layout==&identity && raw && bytes==sizeof(Observation*);}
static bool trace(const void* layout,const void* raw,size_t bytes,XL6_ModuleOwnershipVisit visit,void* user)
{assert(valid(layout,raw,bytes));(void)visit;(void)user;return true;}
static bool dispose(const void* layout,void* raw,size_t bytes)
{
    assert(valid(layout,raw,bytes));Observation* observed=*(Observation**)raw;
    observed->dropThread=xrtThreadCurrentId();++observed->dropped;xrtFree(raw);return true;
}
static const XL6_ClosureEnvironmentOpsV1 captures={sizeof(captures),valid,trace,dispose};
static void invoke(void){}
typedef struct Attempt {XL6_OwnershipPlan* plan;XL6_OwnershipPlanResult result;} Attempt;
static int32 destroyWorker(ptr raw)
{Attempt* attempt=raw;attempt->result=xl6OwnershipPlanTryDestroy(attempt->plan);return 0;}
int main(int argc,char** argv)
{
    assert((argc==2 || (argc==3 && !strcmp(argv[2],"guarded"))) && !xl6Xge3RuntimeBindV1());
    bool guarded=argc==3;Observation observed={.creator=xrtThreadCurrentId()};
    Observation** environment=xrtMalloc(sizeof(*environment));assert(environment);*environment=&observed;
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(NULL,invoke,NULL,environment,sizeof(*environment),&identity,&captures,NULL,NULL,0);assert(closure);
    const XL6_NativeCallbackRuntimeV1* callbacks=xl6NativeCallbackRuntimeV1Get();
    XL6_NativeCallback* callback=callbacks->Create(NULL,closure,NULL,9);assert(callback);
    xui_callback_owner_t owner={callbacks->Ownership(callback),callbacks->HoldNative,callbacks->DropNative};
    xui_proxy_t table=xuiProxyXge();xui_proxy proxy=NULL;
    assert(xuiProxyCreateOwned(&table,&owner,&proxy)==XUI_OK);
    xui_font font=NULL;assert(proxy->fontLoadFile(proxy,&font,argv[1],16.0f,0)==XUI_OK);
    XL6_ForeignView* view=xl6ForeignViewCreateRootWithDomainV1(font,&xl6Xui3FontPolicy,&xl6Xui3OwnershipDomainV1,NULL);assert(view);
    assert(callbacks->Release(callback));xl6ClosureRuntimeV1Get()->Release(closure);
    assert(xuiProxyReleaseChecked(proxy)==XUI_OK);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),xl6OwnershipDomainClosureV1Get(),xl6OwnershipDomainNativeCallbackV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV7(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,NULL,false,catalog,xl6OwnershipCatalogBoundaryV1Get(catalog),&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6ForeignViewCloseV1(view) && xl6ForeignViewReleaseV1(view));
    assert(!observed.dropped);
    Attempt attempt={.plan=plan};xthread* thread=xrtThreadCreate(destroyWorker,&attempt,0);assert(thread);
    assert(xrtThreadWaitFor(thread,10000)==XWAIT_OK && xrtThreadExitCode(thread)==0);xrtThreadDestroy(thread);
    if(guarded){
        assert(attempt.result==XL6_OWNERSHIP_PLAN_BUSY && !observed.dropped);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(observed.dropped==1 && observed.dropThread==observed.creator);
    }else assert(attempt.result==XL6_OWNERSHIP_PLAN_READY && observed.dropped==1 && observed.dropThread!=observed.creator);
    assert(!xl6ForeignViewPendingV1(0));xl6OwnershipCatalogDestroyV1(catalog);
    printf("%s: creator=%llu cleanup=%llu; real XGE CPU font finalization preceded this backend-owner cleanup\n",guarded ? "PASS snapshot retirement affinity (worker BUSY, creator retry)":"REPRODUCED missing snapshot retirement affinity",(unsigned long long)observed.creator,(unsigned long long)observed.dropThread);return 0;
}
