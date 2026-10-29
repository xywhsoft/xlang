#ifndef XL6_X509_VERIFY_ABI_H
#define XL6_X509_VERIFY_ABI_H
XL6_TEXT_API void* xl6X509PublicKeyInfoFromSpkiV1(const xvalue*,int64_t);
XL6_TEXT_API int64_t xl6X509CertificateCheckSignatureV1(void*,void*);
XL6_TEXT_API int64_t xl6X509CertificateCheckSignatureKeyV1(void*,void*);
XL6_TEXT_API void xl6X509CertificateVerifySignatureV1(void*,void*);
XL6_TEXT_API void xl6X509CertificateVerifySignatureKeyV1(void*,void*);
XL6_TEXT_API int64_t xl6X509CrlCheckSignatureV1(void*,void*);
XL6_TEXT_API int64_t xl6X509CrlCheckSignatureKeyV1(void*,void*);
XL6_TEXT_API void xl6X509CrlVerifySignatureV1(void*,void*);
XL6_TEXT_API void xl6X509CrlVerifySignatureKeyV1(void*,void*);
XL6_TEXT_API int64_t xl6X509SignatureInfoCheckBytesV1(void*,const xvalue*,const xvalue*,void*,int64_t);
XL6_TEXT_API void xl6X509SignatureInfoVerifyBytesV1(void*,const xvalue*,const xvalue*,void*,int64_t);
XL6_TEXT_API int64_t xl6X509SignatureInfoCheckTextV1(void*,xstrview,const xvalue*,void*,int64_t);
XL6_TEXT_API void xl6X509SignatureInfoVerifyTextV1(void*,xstrview,const xvalue*,void*,int64_t);
XL6_TEXT_API int64_t xl6X509SignatureInfoCheckBufferV1(void*,void*,const xvalue*,void*,int64_t);
XL6_TEXT_API void xl6X509SignatureInfoVerifyBufferV1(void*,void*,const xvalue*,void*,int64_t);
#endif
