#include "xl6_tls_metadata_internal.h"
#include "xl6_tls_transport_internal.h"
#include "xl6_x509_internal.h"
void* xl6TlsInfoCertificateAtV1(const void* raw,int64_t index)
{
    const XL6_TlsInfoV1* value=xl6TlsInfoCheckedV1(raw);
    if(!value)return NULL;
    if(index<0||(uint64_t)index>=value->certificateCount){
        xrtSetErrorKind(XERR_RANGE);xl6TlsTransportFailedV1();return NULL;
    }
    void* result=xl6X509CertificateCreateSpanV1(value->certificates[index]);
    if(!result)xl6TlsTransportFailedV1();
    return result;
}
