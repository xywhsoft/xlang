#include "include/xruntime_decl.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6ExecutorHandlePolicyV1Get())&&
    xl6OwnershipCatalogAddHandleV1(catalog,xl6ExecutorSnapshotHandlePolicyV1Get());}
XL6_OWNERSHIP_DOMAIN(ExecutorLanguage,load,NULL)
