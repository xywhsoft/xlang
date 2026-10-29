#include "include/xruntime_decl.h"
#include "xl6_ownership_domain_impl.h"
#include "xl6_text_value.h"
#include "xl6_channel_deadline_language.h"
static bool load(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6ChannelDeadlineSendBuilderHandlePolicyV1Get())&&
        xl6OwnershipCatalogAddHandleV1(catalog,xl6ChannelDeadlineReceiveBuilderHandlePolicyV1Get());
}
XL6_OWNERSHIP_DOMAIN(ChannelDirectDeadlineLanguage,load,NULL)
