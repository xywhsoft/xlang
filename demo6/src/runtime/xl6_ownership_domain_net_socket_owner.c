#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_net_socket_owner.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddQuiescenceV1(catalog,xl6NetSocketQuiescenceV1Get());}
XL6_OWNERSHIP_QUERY(NetSocketOwner)
{
    (void)policies;(void)completion;(void)preparation;
    *adapter=xl6NetSocketOwnershipAdapterV1(ref);return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetSocketOwner,load,ownershipDomainNetSocketOwner)
