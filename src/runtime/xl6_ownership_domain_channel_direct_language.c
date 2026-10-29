#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_text_value.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6ChannelSendFutureBuilderHandlePolicyV1Get())&&
        xl6OwnershipCatalogAddHandleV1(catalog,xl6ChannelReceiveFutureBuilderHandlePolicyV1Get());
}
XL6_OWNERSHIP_DOMAIN(ChannelDirectLanguage,load,NULL)
