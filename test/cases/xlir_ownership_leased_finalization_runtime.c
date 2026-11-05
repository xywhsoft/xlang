/* Exercise the original continuation scenarios through a NO-Load leased
 * native domain. Its descriptor is protected by real ForeignView roots, not
 * registered as a permanently resident semantic policy. */
#define main resident_finalization_main
#include "xlir_ownership_finalization_runtime.c"
#undef main
#include "src/runtime/xl6_foreign_view.h"

static unsigned queries,queryMode,destroyed;
static bool nativeQuery(xrtownershipref r,const xrtownershipadapterv1* a,
    const xrtownershipfinalizationv1** out)
{
    *out=NULL;++queries;
    if(r.Ops!=&ops)return true;
    if(queryMode==1){*out=&other;return true;}
    if(queryMode==2){*out=&invalid[0];return true;}
    if(queryMode==3){*out=&capability;return false;}
    if(queryMode==4)return false;
    if(a!=&adapter)return true;
    *out=&capability;return true;
}
static bool conflictQuery(xrtownershipref r,const xrtownershipadapterv1* a,
    const xrtownershipfinalizationv1** out)
{*out=r.Ops==&ops&&a==&adapter?&other:NULL;return true;}
static const XL6_OwnershipDomainV2 leased={{sizeof(leased),NULL,resolve,reads},
    XL6_OWNERSHIP_DOMAIN_V2_TAG,nativeQuery};
static const XL6_OwnershipDomainV2 conflicting={{sizeof(conflicting),NULL,resolve,reads},
    XL6_OWNERSHIP_DOMAIN_V2_TAG,conflictQuery};
static bool destroyHolder(void* raw){assert(raw);++destroyed;return true;}
static const XL6_ForeignViewDescriptorV1 holderPolicy={sizeof(holderPolicy),XL6_FOREIGN_VIEW_V1_TAG,
    "leased-domain-holder",0,1,NULL,NULL,destroyHolder,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
static XL6_ForeignView* holder(const XL6_OwnershipDomainV1* d)
{XL6_ForeignView* result=xl6ForeignViewCreateRootWithDomainV1(&destroyed,&holderPolicy,d,NULL);assert(result);return result;}
static void releaseHolder(XL6_ForeignView* value)
{assert(xl6ForeignViewCloseV1(value));assert(xl6ForeignViewReleaseV1(value));assert(xl6ForeignViewDrainV1());}
static void queriesAndDescriptors(void)
{
    const XL6_OwnershipDomainV1* d[]={&leased.V1};
    Node probeNode={0};const xrtownershipfinalizationv1* result=NULL;
    XL6_OwnershipDomainV2 bad=leased;
    for(unsigned i=0;i<3;++i){
        bad=leased;if(i==0)bad.V1.size=sizeof(bad)+1;
        if(i==1)bad.abiTag=0;
        if(i==2)bad.ResolveFinalization=NULL;
        const XL6_OwnershipDomainV1* invalidDomains[]={&bad.V1};
        unsigned q=queries;
        assert(!xl6OwnershipCatalogCreateV1(invalidDomains,1));assert(queries==q);xrtClearError();
        assert(!xl6ForeignViewCreateRootWithDomainV1(&destroyed,&holderPolicy,&bad.V1,NULL));
        assert(queries==q);xrtClearError();
    }
    catalog=xl6OwnershipCatalogCreateV1(d,1);assert(catalog);
    for(queryMode=0;queryMode<5;++queryMode){
        result=&other;
        bool ok=xl6OwnershipCatalogResolveFinalizationV1(ref(&probeNode),catalog,&adapter,&result);
        assert(ok==(queryMode<2));assert(result==(ok?(queryMode?&other:&capability):NULL));
    }
    queryMode=0;result=&other;
    assert(xl6OwnershipCatalogResolveFinalizationV1((xrtownershipref){0},catalog,&adapter,&result)&&!result);
    xl6OwnershipCatalogDestroyV1(catalog);
    const XL6_OwnershipDomainV1* conflicts[]={&leased.V1,&conflicting.V1};
    catalog=xl6OwnershipCatalogCreateV1(conflicts,2);assert(catalog);result=&other;
    assert(!xl6OwnershipCatalogResolveFinalizationV1(ref(&probeNode),catalog,&adapter,&result)&&!result);
    xl6OwnershipCatalogDestroyV1(catalog);

    /* A resident Load and a leased exact query must agree by identity. */
    selected=&other;const XL6_OwnershipDomainV1* mixed[]={&domain,&leased.V1};
    catalog=xl6OwnershipCatalogCreateV1(mixed,2);assert(catalog);result=&other;
    assert(!xl6OwnershipCatalogResolveFinalizationV1(ref(&probeNode),catalog,&adapter,&result)&&!result);
    xl6OwnershipCatalogDestroyV1(catalog);selected=&capability;

    XL6_ForeignView *first=holder(&leased.V1),*second=holder(&leased.V1);
    const XL6_OwnershipDomainV1* foreign[]={xl6OwnershipDomainForeignViewV1Get()};
    catalog=xl6OwnershipCatalogCreateV1(foreign,1);assert(catalog);queries=0;
    assert(xl6OwnershipCatalogResolveFinalizationV1(ref(&probeNode),catalog,&adapter,&result)&&result==&capability);
    assert(queries==1); /* One query per distinct capability, not per root. */
    XL6_ForeignView* third=holder(&conflicting.V1);result=&other;
    assert(!xl6OwnershipCatalogResolveFinalizationV1(ref(&probeNode),catalog,&adapter,&result)&&!result);
    releaseHolder(third);xl6OwnershipCatalogDestroyV1(catalog);releaseHolder(second);releaseHolder(first);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    queriesAndDescriptors();
    XL6_ForeignView *first=holder(&leased.V1),*second=holder(&leased.V1);
    const XL6_OwnershipDomainV1* d[]={xl6OwnershipDomainForeignViewV1Get()};
    catalog=xl6OwnershipCatalogCreateV1(d,1);assert(catalog);
    for(unsigned i=0;i<10;++i){
        waits();failed(false,false);failed(false,true);failed(true,false);failed(true,true);
        snapshot_failure(false);snapshot_failure(true);new_peer();resurrect();read_busy();
    }
    ambient_error();affinity();legacy_wait();reset();reenter=true;collect(create(cycle()));
    xl6OwnershipCatalogDestroyV1(catalog);releaseHolder(second);releaseHolder(first);xrtClearError();
    xrtMemDebugSnapshot(&after);assert(allocated==freed&&clears==freed);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    printf("Leased native semantic finalization: %u nodes, exact cold query, NULL failure, explicit retry, conflict rejection, V1 compatibility; zero live delta\n",freed);
    return 0;
}
