#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_udp.h"
static bool load(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionUdpFamilyV1Get());}
XL6_OWNERSHIP_DOMAIN(ExecutionUdp,load,NULL)
