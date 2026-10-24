#include "xl6_tls_role_context_internal.h"
void* xl6TlsClientContextWithProtocolsV1(void* raw,const xvalue* values)
{
    if(xrtGetError())return NULL;
    const XL6_TlsClientContextV1* self=raw;XL6_TlsProtocolsV1* protocols=NULL;
    if(!self){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS client context");return NULL;}
    if(!xl6TlsProtocolsReadV1(values,&protocols))return NULL;
    return xl6TlsClientContextFromV1(self->parameters,&self->verifier,protocols);
}
