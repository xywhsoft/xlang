#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
/* Physical pool creator/lease/pin references and immutable native bytes only.
 * Buffer blocks are uniquely owned storage, not independently counted nodes.
 * This provider does not certify opaque payload callbacks or transport state. */
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetBufPoolOwnershipQuiescenceV1Get());}
XL6_OWNERSHIP_QUERY(NetBufferNative)
{
    (void)policies;(void)completion;
    *adapter=xrtNetBufPoolOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetBytesOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetBufferNative,load,ownershipDomainNetBufferNative)
