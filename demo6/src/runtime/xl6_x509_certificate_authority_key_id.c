#include "xl6_x509_profiles_internal.h"
void* xl6X509CertificateAuthorityKeyIdV1(void* raw)
{
    const xx509cert* cert=xl6X509ProfileCertificateV1(raw);xx509authoritykeyid value;xbytesview source;if(!cert)return NULL;
    xx509result status=xrtX509AuthorityKeyId(cert,&value);if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE) && xl6X509ProfileExtensionV1(cert,35,&source)?xl6X509AuthorityKeyIdentifierCopyV1(source):NULL;
}
