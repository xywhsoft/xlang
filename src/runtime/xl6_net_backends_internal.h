#ifndef XL6_NET_BACKENDS_INTERNAL_H
#define XL6_NET_BACKENDS_INTERNAL_H
#include "xl6_text_value.h"
static inline bool xl6NetBackendInfoInternalV1(int64_t value,xnetportbackendinfo* output)
{
    return value>=XNET_PORT_AUTO&&value<=XNET_PORT_SELECT&&
        xrtNetPortBackendInfo((xnetportbackend)value,output);
}
#endif
