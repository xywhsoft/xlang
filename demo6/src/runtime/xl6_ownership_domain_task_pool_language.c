#include "xl6_task_pool_language.h"
#include "xl6_native_handle.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddHandleV1(catalog,xl6TaskPoolHandlePolicyV1Get())&&
    xl6OwnershipCatalogAddHandleV1(catalog,xl6TaskPoolSnapshotHandlePolicyV1Get());}
XL6_OWNERSHIP_DOMAIN(TaskPoolLanguage,load,NULL)
