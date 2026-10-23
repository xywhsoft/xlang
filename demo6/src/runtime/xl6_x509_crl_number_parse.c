#include "xl6_x509_profiles_internal.h"
xvalue* xl6X509CrlNumberParseV1(const xvalue* value,int64_t limit)
{
    xbytesview input,magnitude;
    return xl6X509ProfileInputV1(value,limit,&input) && xl6X509OutcomeV1(xrtX509CrlNumberParse(input,&magnitude))?
        xl6X509BytesV1(magnitude):NULL;
}
