#include "xl6_ws_stream_internal.h"
#include "xl6_ownership_domain_impl.h"
static bool load(XL6_OwnershipCatalog* c)
{return xl6OwnershipCatalogAddTlsStreamEventsV1(c,xrtWsStreamTlsEventsOwnershipV1Get());}
XL6_OWNERSHIP_DOMAIN(WsStreamTls,load,NULL)
