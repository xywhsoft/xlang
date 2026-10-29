#ifndef XL6_X509_IDENTITY_ABI_H
#define XL6_X509_IDENTITY_ABI_H
XL6_TEXT_API bool xl6X509MatchesDnsV1(xstrview,xstrview,int64_t,int64_t);
XL6_TEXT_API bool xl6X509CertificateMatchesHostV1(void*,xstrview,int64_t);
XL6_TEXT_API void* xl6X509CertificateMatchHostV1(void*,xstrview,int64_t);
#endif
