#include "xl6_net_proxy_internal.h"
#include "xl6_native_handle.h"
static void* create(int64_t kind,xstrview host,int64_t port,xbytesview user,xbytesview password,int64_t auth)
{
    if(port<=0||port>UINT16_MAX){xrtSetErrorKind(XERR_RANGE);xl6NetBufferFailedV1();return NULL;}
    if((kind!=XNET_PROXY_SOCKS5&&kind!=XNET_PROXY_HTTP_CONNECT)||auth<XNET_PROXY_AUTH_AUTO||auth>XNET_PROXY_AUTH_OPTIONAL){
        xrtSetErrorKind(XERR_VALUE);xl6NetBufferFailedV1();return NULL;}
    if(!xl6TextSelect(&host,-1)){xl6NetBufferFailedV1();return NULL;}
    xnetproxyconfig config;xrtNetProxyConfigInit(&config);
    config.Type=(xnetproxytype)kind;config.Host=host;config.Port=(uint16)port;
    config.Auth=(xnetproxyauth)auth;config.Username=user;config.Password=password;
    void* result=xrtNetProxyCreate(&config);if(!result)xl6NetBufferFailedV1();return result;
}
void* xl6NetProxyCreateTextV1(int64_t kind,xstrview host,int64_t port,xstrview user,xstrview password,int64_t auth)
{return create(kind,host,port,(xbytesview){(const uint8_t*)user.Data,user.Size},(xbytesview){(const uint8_t*)password.Data,password.Size},auth);}
void* xl6NetProxyCreateBytesV1(int64_t kind,xstrview host,int64_t port,const xvalue* user,const xvalue* password,int64_t auth)
{
    xbytesview u,p;if(!xl6BufferBytesInput(user,-1,&u)||!xl6BufferBytesInput(password,-1,&p)){xl6NetBufferFailedV1();return NULL;}
    return create(kind,host,port,u,p,auth);
}
void xl6NetProxyDestroyV1(void* raw){xrtNetProxyRelease(raw);}
void* xl6NetProxyCloneV1(void* raw)
{void* result=xrtNetProxyRetain(raw);if(!result)xl6NetBufferFailedV1();return result;}
bool xl6NetProxyInfoV1(const void* raw,xnetproxyinfo* info)
{bool ok=xrtNetProxyInfo(raw,info);if(!ok)xl6NetBufferFailedV1();return ok;}
int64_t xl6NetProxyTypeV1(const void* raw){xnetproxyinfo info;return xl6NetProxyInfoV1(raw,&info)?info.Type:0;}
int64_t xl6NetProxyAuthV1(const void* raw){xnetproxyinfo info;return xl6NetProxyInfoV1(raw,&info)?info.Auth:0;}
int64_t xl6NetProxyPortV1(const void* raw){xnetproxyinfo info;return xl6NetProxyInfoV1(raw,&info)?info.Port:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetProxyHandlePolicyV1Get,xl6NetProxyDestroyV1)
