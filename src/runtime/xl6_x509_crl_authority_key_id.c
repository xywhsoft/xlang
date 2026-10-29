#include "xl6_x509_crl_profiles_internal.h"
void* xl6X509CrlAuthorityKeyIdV1(void* raw)
{
    const xx509crl* crl=xl6X509ProfileCrlV1(raw);xx509authoritykeyid value;xbytesview source;if(!crl)return NULL;
    xx509result status=xrtX509CrlAuthorityKeyId(crl,&value);if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE) && xl6X509CrlProfileExtensionV1(crl,35,&source)?xl6X509AuthorityKeyIdentifierCopyV1(source):NULL;
}
