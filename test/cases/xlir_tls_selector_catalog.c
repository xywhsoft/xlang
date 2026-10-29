#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_native_handle.h"
#include <assert.h>
#include <stdio.h>

static size_t observed;
static bool inspect(xrtownershipref ref,const XL6_OwnershipPoliciesV1* p,
    const xrtownershipadapterv1** a,const xrtownershippreparationv1** b,const XL6_OwnershipFinishV1** c)
{
    (void)ref;(void)a;(void)b;(void)c;size_t requests=0,choices=0;
    for(size_t i=0;i<p->HandleCount;++i){
        requests+=p->Handle[i]==xl6TlsServerRequestHandlePolicyV1Get();
        choices+=p->Handle[i]==xl6TlsServerChoiceHandlePolicyV1Get();
        for(size_t j=0;j<i;++j)assert(p->Handle[i]!=p->Handle[j]);
    }
    assert(requests==1&&choices==1);++observed;return false;
}
static const XL6_OwnershipDomainV1 observer={sizeof(observer),NULL,inspect,NULL};
static XL6_OwnershipCatalog* create(void)
{
    enum {capacity=1
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) +1
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    };
    const XL6_OwnershipDomainV1* domains[capacity];size_t n=0;
#define XL6_OWNERSHIP_DOMAIN_ENTRY(Producer, Component, Getter) domains[n++]=Getter();
#include "src/runtime/xl6_ownership_domains.inc"
#undef XL6_OWNERSHIP_DOMAIN_ENTRY
    for(size_t i=0;i<n;++i)for(size_t j=0;j<i;++j)assert(domains[i]!=domains[j]);
    domains[n++]=&observer;assert(n==capacity);return xl6OwnershipCatalogCreateV1(domains,n);
}
static bool unusedCount(const void* raw,size_t* count){(void)raw;(void)count;return false;}
static bool unusedTrace(const void* raw,xrtownershipvisitor visit,void* context)
{(void)raw;(void)visit;(void)context;return false;}
static const xrtownershipops unknown={unusedCount,unusedTrace};
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_OwnershipCatalog* catalog=create();assert(catalog);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* completion=NULL;
    assert(!xl6OwnershipCatalogResolveV1((xrtownershipref){&unknown,&unknown},catalog,&adapter,&preparation,&completion));
    assert(observed==1&&!adapter&&!preparation&&!completion&&!xrtGetError());
    xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
    size_t point=0;
    for(;point<2048;++point){
        xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));catalog=create();
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        assert(hit?!catalog:catalog!=NULL);xrtClearError();xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
        if(!hit)break;
    }
    assert(point>0&&point<2048);
    printf("TLS selector full catalog: exact independent policies, prefixes=%zu balanced: ok\n",point+1);return 0;
}
