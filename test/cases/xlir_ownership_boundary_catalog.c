#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include <assert.h>
#include <stdio.h>
static bool enter(void* context){(void)context;return false;}
static void leave(void* context){(void)context;assert(0);}
static const XL6_OwnershipBoundaryV1 first={sizeof(first),enter,leave,NULL,NULL};
static const XL6_OwnershipBoundaryV1 second={sizeof(second),enter,leave,NULL,NULL};
static const XL6_OwnershipBoundaryV1 invalid={sizeof(invalid)-1,enter,leave,NULL,NULL};
static bool one(XL6_OwnershipCatalog* c)
{ return xl6OwnershipCatalogSetBoundaryV1(c,&first)&&xl6OwnershipCatalogSetBoundaryV1(c,&first); }
static bool two(XL6_OwnershipCatalog* c){return xl6OwnershipCatalogSetBoundaryV1(c,&second);}
static bool bad(XL6_OwnershipCatalog* c){return xl6OwnershipCatalogSetBoundaryV1(c,&invalid);}
int main(void)
{
    const XL6_OwnershipDomainV1 a={sizeof(a),one,NULL,NULL},b={sizeof(b),two,NULL,NULL},d={sizeof(d),bad,NULL,NULL};
    const XL6_OwnershipDomainV1* domains[]={&a,&b,&d};
    XL6_OwnershipCatalog* c=xl6OwnershipCatalogCreateV1(NULL,0);assert(c);
    assert(!xl6OwnershipCatalogBoundaryV1Get(c));xl6OwnershipCatalogDestroyV1(c);
    c=xl6OwnershipCatalogCreateV1(domains,1);assert(c);
    assert(xl6OwnershipCatalogBoundaryV1Get(c)==&first);
    assert(!xl6OwnershipCatalogSetBoundaryV1(c,&second));xrtClearError();
    assert(xl6OwnershipCatalogBoundaryV1Get(c)==&first);xl6OwnershipCatalogDestroyV1(c);
    assert(!xl6OwnershipCatalogCreateV1(domains,2));xrtClearError();
    assert(!xl6OwnershipCatalogCreateV1(domains+2,1));xrtClearError();
    assert(!xl6OwnershipCatalogSetBoundaryV1(NULL,&first));xrtClearError();
    puts("Native boundary catalog: exact identity, conflicting and invalid descriptors, sealed catalog rejection");return 0;
}
