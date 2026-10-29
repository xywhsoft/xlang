#include "xl6_net_resolver_input.h"
void* xl6NetAddressListResolveV1(xstrview input,int64_t port,int64_t family)
{
    uint16 number;if(!xl6NetResolverPortV1(port,&number))return NULL;
    char* host=xl6NetResolverHostV1(input,family);if(!host)return NULL;
    xnetaddrlist* result=xrtNetResolve(host,number,(xnetfamily)family);
    xrtFree(host);return result;
}
