/* A real finalizer starts independently leased work before the next snapshot. */
#include "release/include/xruntime_decl.h"
#include "src/runtime/xl6_static_module.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/runtime/xl6_stack.h"
#include <assert.h>
#include <stdio.h>

static XL6_StaticModule* module;
static xvalue* root;
static XL6_CodeLease* workerLease;
static xthread* worker;
static xatomic32 started,releaseWorker;
static unsigned finalized,finished;
#define STACK_ARGS UINT64_C(102),UINT64_C(202),3,sizeof(xvalue*),xrtTypeValue()
static int32 runWorker(ptr unused)
{
    (void)unused;XL6_CodeScope scope={0};xl6CodeScopeEnter(&scope,workerLease);
    xrtAtomic32Store(&started,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&releaseWorker,XMEMORY_ACQUIRE))xrtThreadYield();
    xl6CodeScopeLeave(&scope);xl6CodeLeaseRelease(workerLease);return 0;
}
static bool trace(XL6_ModuleOwnershipVisit visit,void* context)
{ return visit(xrtValueOwnership(root),context); }
static int finish(void)
{ assert(finalized==1 && root);++finished;xrtValueRelease(root);root=NULL;return 1; }
static XL6_ModulePrepareResult prepare(void) { return XL6_MODULE_PREPARE_READY; }
static XL6_ModuleRetireResult step(void)
{ return finish() ? XL6_MODULE_RETIRE_READY : XL6_MODULE_RETIRE_FAILED; }
static bool finalize(xvalue* object,ptr context)
{
    (void)context;assert(object==root && xl6StackSizeV1(xrtValueObjectGet(object,XRT_STR_LITERAL("links")),STACK_ARGS,false)==1);
    assert(++finalized==1 && xl6CodeLeaseAcquireScoped(xl6StaticModuleOwnerV1(module),&workerLease)==XL6_CODE_LEASE_ACQUIRED);
    worker=xrtThreadCreate(runWorker,NULL,0);assert(worker);double deadline=xrtTimer()+5;
    while(!xrtAtomic32Load(&started,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtThreadYield();}
    return true;
}
static void cycle(XL6_OwnershipCatalog* catalog)
{
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    finalized=finished=0;workerLease=NULL;worker=NULL;xrtAtomic32Store(&started,0,XMEMORY_RELEASE);xrtAtomic32Store(&releaseWorker,0,XMEMORY_RELEASE);
    static const XL6_ModuleRetirementV1 graph={sizeof(graph),0x584C364D525431ULL,trace,finish};
    XL6_StaticModuleContractV1 base={sizeof(base),&graph,prepare,step,xl6OwnershipCatalogResolveV1,catalog,NULL,NULL,NULL};
    XL6_StaticModuleContractV2 checked={sizeof(checked),&base,xl6OwnershipCatalogProbeV1};
    module=xl6StaticModuleCreateV2(&checked);assert(module);
    const XL6_ClassRuntimeV2* classes=xl6ClassRuntimeV2Get();root=classes->Create(xl6StaticModuleOwnerV1(module),finalize);assert(root);
    xvalue* links=xl6StackCreateV1(STACK_ARGS);assert(links && xrtValueObjectSetNew(root,XRT_STR_LITERAL("links"),links));
    assert(classes->CommitTake(root)==root && xl6StackPushV1(links,STACK_ARGS,&root));
    XL6_StaticModuleResultV1 result={false,true};double deadline=xrtTimer()+5;
    while(!xrtAtomic32Load(&started,XMEMORY_ACQUIRE)){
        result=xl6StaticModuleRetireV1(module);assert(!result.ready && result.ok && !xrtGetError() && xrtTimer()<deadline);
    }
    /* The finalizer has returned; its independently admitted worker still has
     * a genuine module lease and code scope. A fresh graph must defer. */
    result=xl6StaticModuleRetireV1(module);assert(!result.ready && result.ok && !xrtGetError() && finalized==1 && !finished && root);
    result=xl6StaticModuleDrainForV1(module,0);assert(!result.ready && result.ok && !xrtGetError());
    xrtAtomic32Store(&releaseWorker,1,XMEMORY_RELEASE);assert(xrtThreadWaitFor(worker,5000)==XWAIT_OK);xrtThreadDestroy(worker);worker=NULL;workerLease=NULL;
    result=xl6StaticModuleDrainForV1(module,5000);assert(result.ready && result.ok && !xrtGetError() && finalized==1 && finished==1 && !root);
    assert(xl6StaticModuleDestroyV1(&module) && !module);xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainClassV1Get(),xl6OwnershipDomainCodeLeaseV1Get(),xl6OwnershipDomainSequenceV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);
    for(unsigned round=0;round<64;++round)cycle(catalog);
    xl6OwnershipCatalogDestroyV1(catalog);puts("Static module quiescence: 64 real finalizer-to-worker handoffs, active code scope is BUSY, exact leases, no permanent failure, once-only finalization, full ledger balanced");return 0;
}
