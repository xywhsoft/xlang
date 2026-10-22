#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_net_file.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionNetFileFamilyV1Get());}
XL6_OWNERSHIP_DOMAIN(ExecutionNetFile,load,NULL)
