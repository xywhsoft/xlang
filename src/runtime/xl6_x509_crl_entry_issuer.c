#include "xl6_x509_crl_profiles_internal.h"
void* xl6X509CrlEntryIssuerV1(void* raw,int64_t limit)
{
    const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);xx509gencursor cursor;
    if(!self || !xl6X509LimitV1(0,limit,"invalid CRL entry issuer budget"))return NULL;
    xx509result status=xrtX509CrlEntryIssuer(&self->value,&cursor);if(status==X509_DONE)return NULL;
    return xl6X509OutcomeV1(status==X509_VALUE)?xl6X509GeneralNamesCopyV1(xl6X509CursorContentV1(cursor.Items),limit):NULL;
}
