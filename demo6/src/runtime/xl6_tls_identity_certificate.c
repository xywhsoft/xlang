#include "xl6_tls_identity_internal.h"
#include "xl6_x509_internal.h"

void* xl6TlsIdentityCertificateV1(void* raw,int64_t index)
{
    if(xrtGetError())return NULL;
    if(!raw){xl6TlsFailV1(XERR_ARGUMENT,"missing TLS identity");return NULL;}
    size_t count=xrtTlsIdentityCertificateCount(raw);
    if(index<0 || (uint64_t)index>=count){xl6TlsFailV1(XERR_RANGE,"TLS certificate index is outside the chain");return NULL;}
    xbytesview certificate={0};
    if(!xl6TlsOutcomeV1(xrtTlsIdentityCertificate(raw,(size_t)index,&certificate)))return NULL;
    return xl6X509CertificateCreateSpanV1(certificate);
}
