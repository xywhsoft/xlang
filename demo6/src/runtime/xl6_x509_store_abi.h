#ifndef XL6_X509_STORE_ABI_H
#define XL6_X509_STORE_ABI_H
#include "xl6_native_class_abi.h"
XL6_TEXT_API void xl6X509StoreDestroyV1(void*);
XL6_TEXT_API void xl6X509StoreSnapshotDestroyV1(void*);
XL6_TEXT_API void* xl6X509StoreCreateV1(void);
XL6_TEXT_API void* xl6X509StoreCloneV1(void*);
XL6_TEXT_API void* xl6X509StoreSnapshotV1(void*);
XL6_TEXT_API int64_t xl6X509StoreCountV1(void*);
XL6_TEXT_API int64_t xl6X509StoreBytesV1(void*);
XL6_TEXT_API void* xl6X509StoreCertificateV1(void*,int64_t);
XL6_TEXT_API void* xl6X509StoreAnchorV1(void*,int64_t);
XL6_TEXT_API bool xl6X509StoreAddV1(void*,void*);
XL6_TEXT_API int64_t xl6X509StoreAddPemV1(void*,xstrview,int64_t);
XL6_TEXT_API int64_t xl6X509StoreMergeV1(void*,void*);
XL6_TEXT_API void xl6X509StoreClearV1(void*);
XL6_TEXT_API int64_t xl6X509StoreSnapshotCountV1(void*);
XL6_TEXT_API int64_t xl6X509StoreSnapshotBytesV1(void*);
XL6_TEXT_API void* xl6X509StoreSnapshotCertificateV1(void*,int64_t);
XL6_TEXT_API void* xl6X509StoreSnapshotAnchorV1(void*,int64_t);
#endif
