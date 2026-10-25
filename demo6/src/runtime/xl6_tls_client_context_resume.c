#include "xl6_tls_role_context_internal.h"
void* xl6TlsClientContextWithResumeV1(void* raw,void* resume)
{
    if(xrtGetError())return NULL;
    const XL6_TlsClientContextV1* self=raw;XL6_TlsProtocolsV1* protocols=NULL;
    if(!self||!resume){xl6TlsFailV1(XERR_ARGUMENT,"TLS client resumption needs a context and credential");return NULL;}
    if(!xl6TlsProtocolsCopyV1(self->protocols,&protocols))return NULL;
    static const XL6_TlsResumeOpsV1 ops={xrtTlsResumeRetain,xrtTlsResumeRelease};
    return xl6TlsClientContextFromResumeV1(self->parameters,&self->verifier,protocols,(XL6_TlsResumeOwnerV1){resume,&ops});
}
