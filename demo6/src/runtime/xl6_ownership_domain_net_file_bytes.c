#include "xl6_text_value.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    const xnetfiledataownershipv1* policy=xl6NetFileBytesInputPolicyV1Get();
    return policy&&xl6OwnershipCatalogAddNetFileInputV1(catalog,policy);
}
XL6_OWNERSHIP_DOMAIN(NetFileBytes,load,NULL)
