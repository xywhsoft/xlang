#include "xl6_x509_identity_internal.h"

bool xl6X509MatchesDnsV1(xstrview pattern,xstrview host,int64_t patternSize,int64_t hostSize)
{
    if(xrtGetError() || !xl6TextSelectRaw(&pattern,patternSize) || !xl6TextSelectRaw(&host,hostSize))return false;
    xx509result result=xrtX509MatchDns(pattern,host);
    return xl6X509IdentityResultV1(result) && result==X509_VALUE;
}
