#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddNetRuntimeV1(catalog,xrtNetFileRuntimeOwnershipV1Get())&&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtNetFileCloseProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddProducerV1(catalog,xrtNetFileFutureProducerPolicyV1Get())&&
        xl6OwnershipCatalogAddPayloadV1(catalog,xrtNetFileFutureBytesPayloadPolicyV1Get())&&
        xl6OwnershipCatalogAddCancelWatchV1(catalog,xrtNetFileFutureCancelPolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetFileOwnershipQuiescenceV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,xrtNetFileFutureOwnershipQuiescenceV1Get());
}
XL6_OWNERSHIP_QUERY(NetFileNative)
{
    (void)completion;
    *adapter=xrtNetFileOwnershipAdapterV1(ref,preparation);
    if(!*adapter)*adapter=xrtNetFileFutureOwnershipAdapterV1(ref,policies->NetFileInput,policies->NetFileInputCount,preparation);
    return *adapter!=NULL;
}
XL6_OWNERSHIP_DOMAIN(NetFileNative,load,ownershipDomainNetFileNative)
