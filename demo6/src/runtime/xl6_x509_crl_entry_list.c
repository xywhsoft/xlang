#include "xl6_x509_crl_profiles_internal.h"
void* xl6X509CrlEntriesV1(void* raw,int64_t limit)
{
    const xx509crl* crl=xl6X509ProfileCrlV1(raw);xx509crlcursor cursor;
    if(!crl || !xl6X509LimitV1(0,limit,"invalid CRL entry budget") || !xl6X509OutcomeV1(xrtX509CrlEntryInit(crl,&cursor)))return NULL;
    return xl6X509CrlEntriesCopyV1(xl6X509CursorContentV1(cursor.Items),crl->Version,limit);
}
