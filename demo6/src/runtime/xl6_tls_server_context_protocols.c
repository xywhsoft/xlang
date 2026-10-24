#include "xl6_tls_role_context_internal.h"
void* xl6TlsServerContextWithProtocolsV1(void* raw,const xvalue* values,bool requireProtocol)
{
    if(xrtGetError())return NULL;
    const XL6_TlsServerContextV1* self=raw;XL6_TlsProtocolsV1* protocols=NULL;
    if(!self){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS server context");return NULL;}
    if(!xl6TlsProtocolsReadV1(values,&protocols))return NULL;
    return xl6TlsServerContextFromV1(self->parameters,self->identity,protocols,requireProtocol);
}
