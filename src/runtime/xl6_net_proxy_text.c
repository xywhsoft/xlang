#include "xl6_net_proxy_internal.h"
xstrview xl6NetProxyHostV1(const void* raw)
{
    xnetproxyinfo info;if(!xl6NetProxyInfoV1(raw,&info))return (xstrview){0};
    xstrview result=xl6TextCopy(info.Host);if(!result.Data)xl6NetBufferFailedV1();return result;
}
