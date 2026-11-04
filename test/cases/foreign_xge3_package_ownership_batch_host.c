#define _GNU_SOURCE 1
#include "foreign_xge3_package_ownership_batch.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_foreign_context.h"
#include <assert.h>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
typedef HMODULE NativeLibrary;
static NativeLibrary open_library(const char* path){return LoadLibraryA(path);}
static void close_library(NativeLibrary library){assert(FreeLibrary(library));}
static void* symbol(NativeLibrary library,const char* name){return (void*)GetProcAddress(library,name);}
static bool loaded(const char* path){return GetModuleHandleA(path)!=NULL;}
#else
#include <dlfcn.h>
typedef void* NativeLibrary;
static NativeLibrary open_library(const char* path){return dlopen(path,RTLD_NOW|RTLD_LOCAL);}
static void close_library(NativeLibrary library){assert(!dlclose(library));}
static void* symbol(NativeLibrary library,const char* name){return dlsym(library,name);}
static bool loaded(const char* path){void* h=dlopen(path,RTLD_NOW|RTLD_NOLOAD);if(!h)return false;dlclose(h);return true;}
#endif
typedef struct Host {
    unsigned refs,acquisitions;
    bool cleared,reverse;
    const void* claim;
    NativeLibrary library;
    xrtownershipref child,retiredChild;
    XL6_CodeLease* lease;
    XL6_CodeLease* retiredLease;
    XL6_OwnershipPlan* dropping;
} Host;
static const xrtownershipops* nativeOps;
static const xrtownershipadapterv1* nativeAdapter;
static unsigned boundaryEnters,boundaryLeaves,order,discoveries;
static bool host_count(const void* raw,size_t* output)
{const Host* h=raw;if(!h || h->cleared || !h->refs || !output)return false;*output=h->refs;return true;}
static bool host_trace(const void* raw,xrtownershipvisitor visit,void* user)
{
    const Host* h=raw;
    if(h->reverse && h->lease && !visit(xl6CodeLeaseOwnership(h->lease),user))return false;
    if(h->child.Data && !visit(h->child,user))return false;
    return h->reverse || !h->lease || visit(xl6CodeLeaseOwnership(h->lease),user);
}
static const xrtownershipops hostOps={host_count,host_trace};
static int acquire(void* raw){Host* h=raw;++h->refs;++h->acquisitions;return 1;}
static void release(void* raw)
{
    Host* h=raw;assert(h->refs && h->acquisitions);
    /* Host callbacks may reenter the same retirement while its caller still
     * has the plan pointer. Its live running guard must reject that retry. */
    if(h->dropping){assert(xl6OwnershipPlanTryDestroy(h->dropping)==XL6_OWNERSHIP_PLAN_ERROR);xrtClearError();}
    --h->refs;if(!--h->acquisitions){close_library(h->library);h->library=NULL;}
}
static bool host_hold(const void* raw){Host* h=(Host*)raw;++h->refs;return true;}
static void host_drop(const void* raw){Host* h=(Host*)raw;assert(h->refs);--h->refs;}
static bool host_claim(const void* raw,const void* token)
{Host* h=(Host*)raw;if(h->claim && h->claim!=token)return false;h->claim=token;return true;}
static void host_restore(const void* raw,const void* token)
{Host* h=(Host*)raw;assert(h->claim==token && !h->cleared);h->claim=NULL;}
static void host_clear(const void* raw,const void* token)
{
    Host* h=(Host*)raw;assert(h->claim==token && !h->cleared);h->cleared=true;
    h->retiredChild=h->child;h->child=(xrtownershipref){0};h->retiredLease=h->lease;h->lease=NULL;
}
static bool host_finish(const void* raw,const void* token)
{
    Host* h=(Host*)raw;assert(h->claim==token && h->cleared);
    xrtownershipref child=h->retiredChild;h->retiredChild=(xrtownershipref){0};
    XL6_CodeLease* lease=h->retiredLease;h->retiredLease=NULL;
    if(child.Data)nativeAdapter->Drop(child.Data);
    xl6CodeLeaseRelease(lease);return true;
}
static const xrtownershipadapterv1 hostAdapter={sizeof(hostAdapter),host_hold,host_drop,host_claim,host_restore,NULL,host_clear,host_finish};
static bool resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{
    (void)policies;(void)preparation;(void)finish;
    if(ref.Ops==&hostOps){*adapter=&hostAdapter;return true;}
    if(ref.Ops==nativeOps){*adapter=nativeAdapter;return true;}
    *adapter=xl6CodeLeaseOwnershipAdapterV1(ref);return *adapter!=NULL;
}
static bool enter(void* raw){(void)raw;++boundaryEnters;order=0;return true;}
static void leave(void* raw){(void)raw;++boundaryLeaves;}
static bool discover_original(XL6_ModuleOwnershipVisit visit,void* user,void* raw)
{(void)visit;(void)user;(void)raw;assert(order==0);order=1;return true;}
static bool discover_added(XL6_ModuleOwnershipVisit visit,void* user,void* raw)
{(void)visit;(void)user;(void)raw;assert(order==1);order=2;++discoveries;return true;}
static const XL6_OwnershipBoundaryV1 boundary={sizeof(boundary),enter,leave,discover_original,NULL};
static const XL6_OwnershipDiscoveryV1 discovery={sizeof(discovery),discover_added,NULL};
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogSetBoundaryV1(catalog,&boundary) && xl6OwnershipCatalogAddDiscoveryV1(catalog,&discovery) && xl6OwnershipCatalogAddDiscoveryV1(catalog,&discovery);}
static const XL6_OwnershipDomainV1 domain={sizeof(domain),load,resolve,NULL};
static unsigned ordinaryReleases;
static int ordinary_acquire(void* unused){(void)unused;return 1;}
static void ordinary_release(void* unused){(void)unused;++ordinaryReleases;}
static void nested_batch(void)
{
    XL6_CodeOwner owner={sizeof(owner),NULL,ordinary_acquire,ordinary_release};XL6_CodeLease* a=NULL;XL6_CodeLease* b=NULL;
    assert(xl6CodeLeaseAcquire(&owner,&a)==XL6_CODE_LEASE_ACQUIRED && xl6CodeLeaseAcquire(&owner,&b)==XL6_CODE_LEASE_ACQUIRED);
    XL6_CodeLeaseReleaseScopeV1 outer={0},inner={0};assert(xl6CodeLeaseReleaseScopeBeginV1(&outer));
    assert(xrtMemDebugFailAfter(0));xl6CodeLeaseRelease(a);assert(!ordinaryReleases);
    assert(xl6CodeLeaseReleaseScopeBeginV1(&inner));xl6CodeLeaseRelease(b);
    xl6CodeLeaseReleaseScopeEndV1(&inner);assert(!ordinaryReleases);
    xl6CodeLeaseReleaseScopeEndV1(&outer);assert(ordinaryReleases==2 && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();
}
int main(int argc,char** argv)
{
    assert(argc==2 && !loaded(argv[1]));nested_batch();
    for(unsigned reverse=0;reverse<2;++reverse){
        Host host={0};NativeBatchNode node={.refs=1};host.reverse=reverse;host.library=open_library(argv[1]);assert(host.library);
        nativeOps=((const xrtownershipops*(*)(void))symbol(host.library,"native_batch_ops"))();
        nativeAdapter=((const xrtownershipadapterv1*(*)(void))symbol(host.library,"native_batch_adapter"))();assert(nativeOps && nativeAdapter);
        host.child=(xrtownershipref){&node,nativeOps};
        XL6_CodeOwnerGraph owner={{sizeof(owner),&host,acquire,release},UINT64_C(0x584C36434F574E31),&hostOps};
        assert(xl6CodeLeaseAcquire(&owner.base,&host.lease)==XL6_CODE_LEASE_ACQUIRED);
        const XL6_OwnershipDomainV1* domains[]={&domain};XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
        XL6_OwnershipPlan* plan=NULL;const XL6_OwnershipBoundaryV1* combined=xl6OwnershipCatalogBoundaryV1Get(catalog);assert(combined && combined!=&boundary);
        assert(xl6OwnershipPlanTryCreateV7((xrtownershipref){&host,&hostOps},xl6OwnershipCatalogProbeV1,NULL,false,catalog,combined,&plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
        assert(loaded(argv[1]) && host.acquisitions==1 && node.refs==1);
        host.dropping=plan;assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);host.dropping=NULL;
        assert(!loaded(argv[1]) && !host.refs && !host.acquisitions && !node.refs && node.drops==2);
        xl6OwnershipCatalogDestroyV1(catalog);
    }
    assert(discoveries==4 && boundaryEnters==boundaryLeaves);
    puts("native descriptor DLL: both graph orders, real unload after all Drop, nested allocation-free batches and composed discovery: PASS");return 0;
}
