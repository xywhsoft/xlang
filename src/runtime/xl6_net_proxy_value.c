#include "xl6_net_proxy_internal.h"
static xvalue* credential(const void* raw,bool password)
{
    xnetproxyinfo info;if(!xl6NetProxyInfoV1(raw,&info))return NULL;
    xvalue* value=xrtValueBytes(password?info.Password:info.Username);if(!value)xl6NetBufferFailedV1();return value;
}
xvalue* xl6NetProxyUsernameV1(const void* raw){return credential(raw,false);}
xvalue* xl6NetProxyPasswordV1(const void* raw){return credential(raw,true);}
