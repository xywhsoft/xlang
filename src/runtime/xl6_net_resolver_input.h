#ifndef XL6_NET_RESOLVER_INPUT_H
#define XL6_NET_RESOLVER_INPUT_H
#include "xl6_text_internal.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
static inline void xl6NetResolverFailedV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
static inline bool xl6NetResolverPortV1(int64_t port,uint16* output)
{if(port<0||port>UINT16_MAX){xrtSetErrorKind(XERR_RANGE);return false;}*output=(uint16)port;return true;}
static inline char* xl6NetResolverHostV1(xstrview input,int64_t family)
{
    if(family!=XNET_FAMILY_UNSPEC&&family!=XNET_FAMILY_IPV4&&family!=XNET_FAMILY_IPV6){xrtSetErrorKind(XERR_VALUE);return NULL;}
    if(!xl6TextSelect(&input,-1))return NULL;
    if(!input.Size||memchr(input.Data,0,input.Size)){xrtSetErrorKind(XERR_VALUE);return NULL;}
    return xrtStrDupView(input);
}
#endif
