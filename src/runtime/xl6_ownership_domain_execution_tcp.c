#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_execution_tcp.h"
/* Generic execution admission consumes the complete selected typed family
 * set. This provider grants exact identities, never another resource view. */
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionTcpStreamFamilyV1Get())&&
        xl6OwnershipCatalogAddExecutionFamilyV1(catalog,xl6ExecutionTcpListenerFamilyV1Get());
}
XL6_OWNERSHIP_DOMAIN(ExecutionTcp,load,NULL)
