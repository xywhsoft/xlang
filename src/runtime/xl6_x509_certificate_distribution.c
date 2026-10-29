#include "xl6_x509_profiles_internal.h"
static void* points(void* raw,int64_t limit,bool freshest)
{
    const xx509cert* cert=xl6X509ProfileCertificateV1(raw);xx509distributioncursor cursor;
    if(!cert || !xl6X509LimitV1(0,limit,"invalid distribution point budget"))return NULL;
    xx509result status=freshest?xrtX509FreshestCrl(cert,&cursor):xrtX509CrlPoints(cert,&cursor);
    if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE)?xl6X509DistributionPointsCopyV1(xl6X509CursorContentV1(cursor.Items),limit):NULL;
}
void* xl6X509CertificateCrlPointsV1(void* raw,int64_t limit){return points(raw,limit,false);}
void* xl6X509CertificateFreshestCrlV1(void* raw,int64_t limit){return points(raw,limit,true);}
