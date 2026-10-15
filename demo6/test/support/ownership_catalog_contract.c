#include "release/include/xruntime_decl.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t observed[11],calls;
static bool inspect(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    (void)ref;(void)adapter;(void)preparation;(void)completion;size_t at=0;
#define OBSERVE(Name, Type) observed[at++]=policies->Name##Count;
    XL6_OWNERSHIP_POLICY_GROUPS(OBSERVE)
#undef OBSERVE
    return false;
}
static const XL6_OwnershipDomainV1 observer={sizeof(observer),NULL,inspect,NULL};
static const XL6_OwnershipDomainV1* const* allDomains(size_t* count)
{
    enum { capacity=1
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) +1
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    };
    static const XL6_OwnershipDomainV1* domains[capacity];size_t n=0;
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) domains[n++]=Getter();
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    assert(n==75);domains[n++]=&observer;*count=n;return domains;
}
static bool forbiddenCount(const void* data,size_t* count)
{ (void)data;(void)count;++calls;return false; }
static bool forbiddenTrace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data;(void)visit;(void)context;++calls;return false; }
static const xrtownershipops forbidden={forbiddenCount,forbiddenTrace};
static bool readLeaf(xrtownershipref ref)
{ return xrtValueOwnershipAdapterV1(ref)!=NULL; }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void admit(XL6_OwnershipCatalog* catalog)
{
    xvalue* value=xrtValueInt(7);assert(value);
    /* Read certification is independent of retirement adapter discovery.
     * A normal value resolves in the empty catalog but is not thereby given
     * an opaque read capability. A read-only domain need not load policies. */
    XL6_OwnershipCatalog* readCatalog=xl6OwnershipCatalogCreateV1(NULL,0);assert(readCatalog);
    assert(!xl6OwnershipCatalogAdmitsReadV1(xrtValueOwnership(value),readCatalog));
    xl6OwnershipCatalogDestroyV1(readCatalog);
    const XL6_OwnershipDomainV1 readOnly={sizeof(readOnly),NULL,NULL,readLeaf};
    const XL6_OwnershipDomainV1* readDomains[]={&readOnly};
    readCatalog=xl6OwnershipCatalogCreateV1(readDomains,1);assert(readCatalog);
    assert(xl6OwnershipCatalogAdmitsReadV1(xrtValueOwnership(value),readCatalog));
    calls=0;assert(!xl6OwnershipCatalogAdmitsReadV1((xrtownershipref){(void*)(uintptr_t)1,&forbidden},readCatalog) && !calls);
    xl6OwnershipCatalogDestroyV1(readCatalog);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6OwnershipCatalogResolveV1(xrtValueOwnership(value),catalog,&adapter,&preparation,&completion));
    assert(adapter==xrtValueOwnershipAdapterV1(xrtValueOwnership(value)) && !preparation && !completion);
    calls=0;adapter=NULL;
    assert(!xl6OwnershipCatalogResolveV1((xrtownershipref){(void*)(uintptr_t)1,&forbidden},catalog,&adapter,&preparation,&completion));
    assert(!calls && !adapter && !preparation && !completion);
    assert(xrtOwnershipScopeEnd(&freeze));
    xrtValueRelease(value);
}
static bool swallowedLoad(XL6_OwnershipCatalog* catalog)
{
    if(!xl6OwnershipCatalogAddSequenceV1(catalog,xl6ArrayFutureAdmissionOpsV2Get()))xrtClearError();
    return true; /* Deliberately bad producer: ignored OOM must still fail Create. */
}
static bool nullFailure(XL6_OwnershipCatalog* catalog)
{ (void)catalog;xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});return true; }
static unsigned queryMode;
static XL6_OwnershipStepResult neverFinish(const void* data,const void* token)
{ (void)data;(void)token;assert(0);return XL6_OWNERSHIP_STEP_FAILED; }
static bool invalidQuery(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    (void)policies;(void)preparation;
    if(queryMode==0)return true; /* Known without an adapter. */
    *adapter=xrtValueOwnershipAdapterV1(ref);
    if(queryMode==1)return false; /* Unknown with a partial output. */
    static XL6_OwnershipFinishV1 conflict={sizeof(conflict),NULL,neverFinish};
    conflict.Adapter=*adapter;*completion=&conflict;return true;
}
static void basic(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);size_t count=0;
    const XL6_OwnershipDomainV1* const* domains=allDomains(&count);
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,count);assert(catalog);admit(catalog);
    const size_t expected[]={5,48,10,6,3,1,3,1,1,1,1};
    assert(sizeof(expected)==sizeof(observed) && !memcmp(expected,observed,sizeof(expected)));
    assert(!xl6OwnershipCatalogAddSequenceV1(catalog,xl6ArrayFutureAdmissionOpsV2Get()));xrtClearError();admit(catalog);
    xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
    catalog=xl6OwnershipCatalogCreateV1(NULL,0);assert(catalog);admit(catalog);xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
    const XL6_OwnershipDomainV1* duplicate[]={&observer,&observer};
    assert(!xl6OwnershipCatalogCreateV1(duplicate,2));balanced(&before);
    assert(!xl6OwnershipCatalogCreateV1(NULL,1));balanced(&before);
    const XL6_OwnershipDomainV1 bad={0,NULL,inspect,NULL};const XL6_OwnershipDomainV1* one[]={&bad};
    assert(!xl6OwnershipCatalogCreateV1(one,1));balanced(&before);
    const XL6_OwnershipDomainV1 fail={sizeof(fail),nullFailure,NULL,NULL};one[0]=&fail;
    assert(!xl6OwnershipCatalogCreateV1(one,1) && !xrtGetError());balanced(&before);
    xrtSetErrorKind(XERR_STATE);xerror* incoming=xrtErrorRef(xrtGetError());assert(incoming);
    assert(!xl6OwnershipCatalogCreateV1(one,1) && xrtGetError()==incoming);
    xrtErrorFree(incoming);balanced(&before);
    xl6_error_scope outer={0};assert(xl6ErrorScopeBegin(&outer));
    xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    xrtSetErrorKind(XERR_ARGUMENT);
    assert(!xl6OwnershipCatalogCreateV1(one,1));
    xl6_error_result first=xl6ErrorScopeEnd(&outer);assert(!first.ok && !first.error);balanced(&before);
    const XL6_OwnershipDomainV1 invalid={sizeof(invalid),NULL,invalidQuery,NULL};one[0]=&invalid;
    catalog=xl6OwnershipCatalogCreateV1(one,1);assert(catalog);
    xvalue* value=xrtValueInt(7);assert(value);
    for(queryMode=0;queryMode<3;++queryMode){
        xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
        const XL6_OwnershipFinishV1* completion=NULL;
        assert(!xl6OwnershipCatalogResolveV1(xrtValueOwnership(value),catalog,&adapter,&preparation,&completion));
        assert(!adapter && !preparation && !completion && xrtGetError());
        assert(xrtOwnershipScopeEnd(&freeze));xrtClearError();
    }
    xrtValueRelease(value);xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
    puts("ownership catalog: complete typed population, sealed admission, unknown callbacks untouched, conflicting contracts refused");
}
static void oom(bool ignored)
{
    const XL6_OwnershipDomainV1 swallowed={sizeof(swallowed),swallowedLoad,NULL,NULL};
    const XL6_OwnershipDomainV1* one[]={&swallowed};size_t count=0;
    const XL6_OwnershipDomainV1* const* domains=ignored ? one : allDomains(&count);if(ignored)count=1;
    size_t point=0;
    for(;point<128;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,count);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        assert(hit ? !catalog : catalog!=NULL);xrtClearError();
        if(catalog){admit(catalog);xl6OwnershipCatalogDestroyV1(catalog);}balanced(&before);
        if(!hit)break;
    }
    assert(point<128);printf("ownership catalog %s prefixes=%zu first-no-hit=%zu balanced\n",ignored ? "ignored-failure" : "full",point+1,point);
}
static void plans(void)
{
    XL6_ArtifactRuntimeSet roots=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_SLOT);
    XL6_ArtifactRuntimePlan base={0};XL6_ArtifactOwnershipPlan plan={0};
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base) && xl6ArtifactOwnershipPlanBuild(&base,&plan));
    assert(!plan.domainCount && plan.runtime.sourceCount==3);
    assert(!xl6ArtifactRuntimeSetHas(&base.components,XL6_ARTIFACT_OWNERSHIP_CATALOG));
    assert(xl6ComponentSetAdd(roots.words,XL6_ARTIFACT_RUNTIME_COUNT,XL6_ARTIFACT_STACK));
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base) && xl6ArtifactOwnershipPlanBuild(&base,&plan));
    assert(plan.domainCount==1 && !strcmp(plan.domains[0],"xl6OwnershipDomainSequenceV1Get"));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_NETWORK_SERVICE));
    assert(!xl6ArtifactRuntimeSetHas(&plan.runtime.components,XL6_ARTIFACT_FUTURE_PAYLOAD));
    base.sources[0]="src/runtime/not_the_verified_provider.c";
    memset(&plan,0xA5,sizeof(plan));assert(!xl6ArtifactOwnershipPlanBuild(&base,&plan));
    XL6_ArtifactOwnershipPlan empty={0};assert(!memcmp(&plan,&empty,sizeof(plan)));
    assert(xl6ArtifactRuntimeClosePlan(&roots,&base));
    base.components=xl6ArtifactRuntimeSetSingleton(XL6_ARTIFACT_STACK);
    memset(&plan,0xA5,sizeof(plan));assert(!xl6ArtifactOwnershipPlanBuild(&base,&plan));
    assert(!memcmp(&plan,&empty,sizeof(plan)));
    puts("ownership policy plan: exact closure, no fallback or network/Future over-selection, corrupt input leaves zero output");
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));
    if(!strcmp(argv[1],"basic"))basic();
    else if(!strcmp(argv[1],"oom"))oom(false);
    else if(!strcmp(argv[1],"ignored-oom"))oom(true);
    else if(!strcmp(argv[1],"plans"))plans();
    else assert(0);
    return 0;
}
