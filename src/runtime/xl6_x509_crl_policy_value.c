#include "xl6_x509_crl_policy_internal.h"
bool xl6X509PolicyRetainV1(xatomic64* references)
{
    uint64 count=xrtAtomic64Load(references,XMEMORY_RELAXED);
    for(;;) {
        if(!count || count==UINT64_MAX)
            return xl6X509FailV1(XERR_RANGE,"X.509 native owner count is exhausted");
        if(xrtAtomic64CompareExchange(references,&count,count+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;
    }
}
bool xl6X509CrlPolicyConfigV1(int64_t time,int64_t skew,uint64_t flags,
    xx509crlcriticalproc critical,void* data,xx509crlconfig* output)
{
    const uint64 known=X509_CRL_REQUIRE_NEXT_UPDATE|X509_CRL_REQUIRE_NUMBER|
        X509_CRL_REQUIRE_AUTHORITY_KEY_ID|X509_CRL_REQUIRE_KEY_IDENTIFIER|
        X509_CRL_REQUIRE_KEY_USAGE|X509_CRL_ALLOW_SHA1;
    if(!output)return xl6X509FailV1(XERR_ARGUMENT,"missing CRL policy output");
    if(skew<0)return xl6X509FailV1(XERR_RANGE,"CRL clock skew must be nonnegative microseconds");
    if((flags&~known) || ((flags&X509_CRL_REQUIRE_KEY_IDENTIFIER) && !(flags&X509_CRL_REQUIRE_AUTHORITY_KEY_ID)))
        return xl6X509FailV1(XERR_ARGUMENT,"invalid CRL policy flags");
    *output=(xx509crlconfig){.Time=time,.Skew=(uint64)skew,.Flags=(uint32)flags,.Critical=critical,.UserData=data};
    return true;
}
