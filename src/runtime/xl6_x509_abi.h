#ifndef XL6_X509_ABI_H
#define XL6_X509_ABI_H
XL6_TEXT_API void* xl6X509CertificateFromDerV1(const xvalue*,int64_t);
XL6_TEXT_API void* xl6X509CertificateFromPemV1(xstrview,int64_t,int64_t);
XL6_TEXT_API void xl6X509CertificateDestroyV1(void*);
XL6_TEXT_API xvalue* xl6X509CertificateToDerV1(void*);
XL6_TEXT_API xstrview xl6X509CertificateToPemV1(void*,int64_t);
XL6_TEXT_API int64_t xl6X509CertificateVersionV1(void*);
XL6_TEXT_API int64_t xl6X509CertificateNotBeforeV1(void*);
XL6_TEXT_API int64_t xl6X509CertificateNotAfterV1(void*);
XL6_TEXT_API bool xl6X509CertificateValidAtV1(void*,int64_t);
XL6_TEXT_API xvalue* xl6X509CertificateSerialV1(void*);
XL6_TEXT_API xvalue* xl6X509CertificateTbsV1(void*);
XL6_TEXT_API xvalue* xl6X509CertificateIssuerV1(void*);
XL6_TEXT_API xvalue* xl6X509CertificateSubjectV1(void*);
XL6_TEXT_API xvalue* xl6X509CertificateSpkiV1(void*);
XL6_TEXT_API xvalue* xl6X509CertificateExtensionsV1(void*);
XL6_TEXT_API xvalue* xl6X509CertificateSignatureV1(void*);
XL6_TEXT_API xvalue* xl6X509CertificateSignatureOidV1(void*);
XL6_TEXT_API xvalue* xl6X509CertificateSignatureParametersV1(void*);
XL6_TEXT_API void* xl6X509CrlFromDerV1(const xvalue*,int64_t);
XL6_TEXT_API void* xl6X509CrlFromPemV1(xstrview,int64_t,int64_t);
XL6_TEXT_API void xl6X509CrlDestroyV1(void*);
XL6_TEXT_API xvalue* xl6X509CrlToDerV1(void*);
XL6_TEXT_API xstrview xl6X509CrlToPemV1(void*,int64_t);
XL6_TEXT_API int64_t xl6X509CrlVersionV1(void*);
XL6_TEXT_API int64_t xl6X509CrlThisUpdateV1(void*);
XL6_TEXT_API bool xl6X509CrlHasNextUpdateV1(void*);
XL6_TEXT_API int64_t xl6X509CrlNextUpdateV1(void*);
XL6_TEXT_API bool xl6X509CrlValidAtV1(void*,int64_t);
XL6_TEXT_API xvalue* xl6X509CrlTbsV1(void*);
XL6_TEXT_API xvalue* xl6X509CrlIssuerV1(void*);
XL6_TEXT_API xvalue* xl6X509CrlRevokedV1(void*);
XL6_TEXT_API xvalue* xl6X509CrlExtensionsV1(void*);
XL6_TEXT_API xvalue* xl6X509CrlSignatureV1(void*);
XL6_TEXT_API xvalue* xl6X509CrlSignatureOidV1(void*);
XL6_TEXT_API xvalue* xl6X509CrlSignatureParametersV1(void*);
#endif
