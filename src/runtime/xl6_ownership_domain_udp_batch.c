#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_native_handle.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6UdpBatchHandlePolicyV1Get());}
XL6_OWNERSHIP_QUERY(UdpBatch)
{
    (void)policies;(void)preparation;(void)completion;
    *adapter=xrtNetUdpBatchOwnershipAdapterV1(ref);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(UdpBatch,load,ownershipDomainUdpBatch)
