#include "xl6_x509_profiles_internal.h"
int64_t xl6X509CrlReasonParseV1(const xvalue* value,int64_t limit)
{
    xbytesview input;xx509crlreason reason;
    return xl6X509ProfileInputV1(value,limit,&input) && xl6X509OutcomeV1(xrtX509CrlReasonParse(input,&reason))?(int64_t)reason:0;
}
