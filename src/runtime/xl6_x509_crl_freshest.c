#include "xl6_x509_crl_profiles_internal.h"
void* xl6X509CrlFreshestV1(void* raw,int64_t limit)
{
    const xx509crl* crl=xl6X509ProfileCrlV1(raw);xx509distributioncursor cursor;
    if(!crl || !xl6X509LimitV1(0,limit,"invalid CRL freshest distribution budget"))return NULL;
    xx509result status=xrtX509CrlFreshest(crl,&cursor);if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE)?xl6X509DistributionPointsCopyV1(xl6X509CursorContentV1(cursor.Items),limit):NULL;
}
