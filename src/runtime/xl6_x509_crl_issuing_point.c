#include "xl6_x509_crl_profiles_internal.h"
void* xl6X509CrlIssuingPointV1(void* raw)
{
    const xx509crl* crl=xl6X509ProfileCrlV1(raw);xx509issuingpoint value;xbytesview source;if(!crl)return NULL;
    xx509result status=xrtX509CrlIssuingPoint(crl,&value);if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE) && xl6X509CrlProfileExtensionV1(crl,28,&source)?xl6X509IssuingDistributionPointCopyV1(source):NULL;
}
