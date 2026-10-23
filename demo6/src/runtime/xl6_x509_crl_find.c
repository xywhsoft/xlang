#include "xl6_x509_crl_profiles_internal.h"
void* xl6X509CrlFindSerialV1(void* raw,const xvalue* serial)
{
    const xx509crl* crl=xl6X509ProfileCrlV1(raw);xbytesview query;xx509crlentry entry;
    if(!crl || !xl6X509OutcomeV1(xrtValueGetBytes(serial,&query)))return NULL;
    xx509result status=xrtX509CrlFind(crl,query,&entry);if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE)?xl6X509CrlEntryCopyV1(entry.Raw,crl->Version):NULL;
}
