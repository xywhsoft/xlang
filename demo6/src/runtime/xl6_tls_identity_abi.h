#ifndef XL6_TLS_IDENTITY_ABI_H
#define XL6_TLS_IDENTITY_ABI_H
XL6_TEXT_API void xl6TlsIdentityDestroyV1(void*);
XL6_TEXT_API int64_t xl6TlsIdentityKindV1(void*);
XL6_TEXT_API int64_t xl6TlsIdentityCountV1(void*);
XL6_TEXT_API void* xl6TlsIdentityCertificateV1(void*,int64_t);
XL6_TEXT_API void* xl6TlsIdentityRsaV1(XL6_NativeClassArrayContextV1,const xvalue*,void*,int64_t,int64_t);
XL6_TEXT_API void* xl6TlsIdentityP256V1(XL6_NativeClassArrayContextV1,const xvalue*,void*,int64_t,int64_t);
XL6_TEXT_API void* xl6TlsIdentityP384V1(XL6_NativeClassArrayContextV1,const xvalue*,void*,int64_t,int64_t);
XL6_TEXT_API void* xl6TlsIdentityEd25519V1(XL6_NativeClassArrayContextV1,const xvalue*,void*,int64_t,int64_t);
#endif
