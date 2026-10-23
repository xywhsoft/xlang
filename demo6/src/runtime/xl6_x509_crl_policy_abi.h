#ifndef XL6_X509_CRL_POLICY_ABI_H
#define XL6_X509_CRL_POLICY_ABI_H
XL6_TEXT_API void xl6X509ValidatedCrlDestroyV1(void*);
XL6_TEXT_API void xl6X509CrlSetDestroyV1(void*);
XL6_TEXT_API void xl6X509RevocationDestroyV1(void*);
XL6_TEXT_API void xl6X509RevocationCheckDestroyV1(void*);
XL6_TEXT_API void xl6X509CrlCriticalDestroyV1(void*);
XL6_TEXT_API void* xl6X509CrlValidateV1(void*,void*,int64_t,int64_t,int64_t);
XL6_TEXT_API void* xl6X509CrlValidateCallbackV1(void*,void*,int64_t,int64_t,int64_t,void*,struct xlir_closure*);
XL6_TEXT_API void* xl6X509CrlSetCreateV1(void*,void*);
XL6_TEXT_API void* xl6X509ValidatedCrlQueryV1(void*,void*);
XL6_TEXT_API void* xl6X509ValidatedCrlCrlV1(void*);
XL6_TEXT_API void* xl6X509ValidatedCrlIssuerV1(void*);
XL6_TEXT_API int64_t xl6X509ValidatedCrlTimeV1(void*);
XL6_TEXT_API uint64_t xl6X509ValidatedCrlSkewV1(void*);
XL6_TEXT_API int64_t xl6X509ValidatedCrlFlagsV1(void*);
XL6_TEXT_API bool xl6X509ValidatedCrlDeltaV1(void*);
XL6_TEXT_API void* xl6X509CrlSetQueryV1(void*,void*);
XL6_TEXT_API void* xl6X509CrlSetBaseV1(void*);
XL6_TEXT_API void* xl6X509CrlSetDeltaV1(void*);
XL6_TEXT_API int64_t xl6X509RevocationStatusV1(void*);
XL6_TEXT_API int64_t xl6X509RevocationReasonsV1(void*);
XL6_TEXT_API bool xl6X509RevocationObservedV1(void*);
XL6_TEXT_API void* xl6X509RevocationEntryV1(void*);
XL6_TEXT_API void* xl6X509RevocationCheckCreateV1(void*);
XL6_TEXT_API void* xl6X509RevocationCheckWithCrlV1(void*,void*);
XL6_TEXT_API void* xl6X509RevocationCheckWithSetV1(void*,void*);
XL6_TEXT_API void* xl6X509RevocationCheckResultV1(void*);
XL6_TEXT_API bool xl6X509RevocationCheckDeterminedV1(void*);
XL6_TEXT_API void* xl6X509CrlCriticalCreateV1(void);
XL6_TEXT_API void* xl6X509CrlCriticalEntryV1(void*);
XL6_TEXT_API void* xl6X509CrlCriticalExtensionV1(void*);
#endif
