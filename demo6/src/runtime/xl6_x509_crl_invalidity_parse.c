#include "xl6_x509_profiles_internal.h"
int64_t xl6X509CrlInvalidityDateParseV1(const xvalue* value,int64_t limit)
{
    xbytesview input;xtime date;
    return xl6X509ProfileInputV1(value,limit,&input) && xl6X509OutcomeV1(xrtX509CrlInvalidityDateParse(input,&date))?date:0;
}
