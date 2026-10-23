#include "xl6_x509_crl_profiles_internal.h"
bool xl6X509CrlEntryHasReasonV1(void* raw)
{
    const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return false;
    xx509crlreason reason;xx509result status=xrtX509CrlEntryReason(&self->value,&reason);
    return status!=X509_DONE && xl6X509OutcomeV1(status==X509_VALUE);
}
int64_t xl6X509CrlEntryReasonV1(void* raw)
{
    const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return 0;
    xx509crlreason reason;xx509result status=xrtX509CrlEntryReason(&self->value,&reason);
    if(status==X509_DONE){xl6X509FailV1(XERR_STATE,"CRL entry has no explicit reason");return 0;}
    return xl6X509OutcomeV1(status==X509_VALUE)?(int64_t)reason:0;
}
bool xl6X509CrlEntryHasInvalidityDateV1(void* raw)
{
    const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return false;
    xtime date;xx509result status=xrtX509CrlEntryInvalidityDate(&self->value,&date);
    return status!=X509_DONE && xl6X509OutcomeV1(status==X509_VALUE);
}
int64_t xl6X509CrlEntryInvalidityDateV1(void* raw)
{
    const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return 0;
    xtime date;xx509result status=xrtX509CrlEntryInvalidityDate(&self->value,&date);
    if(status==X509_DONE){xl6X509FailV1(XERR_STATE,"CRL entry has no invalidity date");return 0;}
    return xl6X509OutcomeV1(status==X509_VALUE)?date:0;
}
