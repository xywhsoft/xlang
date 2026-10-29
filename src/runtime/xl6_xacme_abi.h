#ifndef XL6_XACME_ABI_H
#define XL6_XACME_ABI_H
/* S string inputs have the language ABI's Data[Size] NUL sentinel. Native
 * adapters validate embedded NUL, then borrow the original bytes. Config and
 * domain arrays are explicit data snapshots, never opaque typed containers. */
XL6_TEXT_API void* xl6AcmeConfigCreateV1(const xvalue*,const xvalue*,int64_t,int64_t,int64_t);
XL6_TEXT_API void xl6AcmeConfigEngineV1(void*,void*);
XL6_TEXT_API void* xl6AcmeClientCreateV1(void*);
XL6_TEXT_API void xl6AcmeDropV1(void*);
XL6_TEXT_API bool xl6AcmeCleanupV1(void*);
XL6_TEXT_API char* xl6AcmeAccountPemV1(void*);
/* Canonical upstream functions: no native or language forwarding layer. */
XL6_TEXT_API char* xrtAcmeStoreLoadAccount(const char*,const char*);
XL6_TEXT_API char* xrtAcmeStoreLoadCert(const char*,const char*);
XL6_TEXT_API char* xrtAcmeStoreLoadCertCa(const char*,const char*);
XL6_TEXT_API void* xl6AcmeProviderCustomV1(xstrview,int64_t,const xvalue*,const xvalue*,const xvalue*);
XL6_TEXT_API void* xl6AcmeProviderCreateV1(int64_t,const xvalue*,void*);
XL6_TEXT_API bool xl6AcmeProviderValidateV1(void*);
XL6_TEXT_API bool xl6AcmeProviderCallV1(void*,int64_t,xstrview,xstrview);
XL6_TEXT_API xstrview xl6AcmeProviderIdV1(void*);
XL6_TEXT_API int64_t xl6AcmeProviderCapsV1(void*);
XL6_TEXT_API int64_t xl6AcmeProviderCountV1(void);
XL6_TEXT_API xstrview xl6AcmeProviderRegistryIdV1(int64_t);
XL6_TEXT_API void* xl6AcmeIssueV1(void*,const xvalue*,void*,bool,xstrview,int64_t,bool);
XL6_TEXT_API void* xl6AcmeGrantTakeV1(void*,bool);
XL6_TEXT_API bool xl6AcmeGrantRenewedV1(void*);
XL6_TEXT_API bool xl6AcmeClientControlV1(void*,int64_t,xstrview,xstrview,int64_t);
XL6_TEXT_API void* xl6AcmeStoreLoadV1(xstrview,xstrview,int64_t);
XL6_TEXT_API bool xl6AcmeStoreSaveV1(xstrview,xstrview,xstrview,xstrview,xstrview,int64_t);
XL6_TEXT_API bool xl6AcmeStoreNeedRenewV1(xstrview,xstrview,int64_t);
XL6_TEXT_API xvalue* xl6AcmeStoreListDomainsV1(xstrview,int64_t);
XL6_TEXT_API void* xl6AcmeObtainV1(void*,const xvalue*,void*,xstrview,int64_t);
XL6_TEXT_API int64_t xl6AcmeCleanupPendingV1(int64_t);
#endif
