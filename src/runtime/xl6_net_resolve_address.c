#include "xl6_text_internal.h"
#include <string.h>

/* Separate DNS component: numeric NetAddress factories do not select it. */
void* xl6NetResolveAddressV1(xstrview text,int64_t port)
{
    if(port<0 || port>UINT16_MAX){xrtSetErrorKind(XERR_RANGE);return NULL;}
    if(!xl6TextSelect(&text,-1))return NULL;
    if(!text.Size || memchr(text.Data,0,text.Size)){xrtSetErrorKind(XERR_VALUE);return NULL;}
    char* host=xrtStrDupView(text);if(!host)return NULL;
    xnetaddr address;bool ok=xrtNetResolveOne(&address,host,(uint16)port,XNET_FAMILY_UNSPEC);
    xrtFree(host);return ok?xl6NetAddressCopyV1(&address):NULL;
}
