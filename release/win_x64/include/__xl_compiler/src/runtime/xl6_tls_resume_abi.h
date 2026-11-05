#ifndef XL6_TLS_RESUME_ABI_H
#define XL6_TLS_RESUME_ABI_H
/* Managed, immutable native credential. No source-language raw-handle import. */
XL6_TEXT_API void* xl6TlsResumeFromPartsV1(int64_t,int64_t,const xvalue*,const xvalue*,int64_t,int64_t,uint64_t,xstrview,const xvalue*,const xvalue*,uint64_t);
XL6_TEXT_API void xl6TlsResumeDestroyV1(void*);
XL6_TEXT_API int64_t xl6TlsResumeVersionV1(const void*);
XL6_TEXT_API int64_t xl6TlsResumeCipherV1(const void*);
XL6_TEXT_API xvalue* xl6TlsResumeTicketV1(const void*);
XL6_TEXT_API xvalue* xl6TlsResumeExportSecretV1(const void*);
XL6_TEXT_API xstrview xl6TlsResumeServerNameV1(const void*);
XL6_TEXT_API xvalue* xl6TlsResumeProtocolV1(const void*);
XL6_TEXT_API xvalue* xl6TlsResumePeerIdentityV1(const void*);
XL6_TEXT_API int64_t xl6TlsResumeLifetimeV1(const void*);
XL6_TEXT_API uint64_t xl6TlsResumeAgeAddV1(const void*);
XL6_TEXT_API uint64_t xl6TlsResumeMaxEarlyDataV1(const void*);
XL6_TEXT_API int64_t xl6TlsResumeIssuedAtV1(const void*);
XL6_TEXT_API int64_t xl6TlsResumeExpiresAtV1(const void*);
XL6_TEXT_API bool xl6TlsResumeValidAtV1(const void*,int64_t);
XL6_TEXT_API uint64_t xl6TlsResumeTicketAgeV1(const void*,int64_t);
XL6_TEXT_API int64_t xl6TlsResumePeerCertificateCountV1(const void*);
XL6_TEXT_API void* xl6TlsResumePeerCertificateV1(const void*,int64_t);
XL6_TEXT_API void* xl6TlsResumeWithPeerCertificatesV1(XL6_NativeClassArrayContextV1,void*,const xvalue*,int64_t,int64_t);
#endif
