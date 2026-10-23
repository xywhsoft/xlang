#ifndef XL6_X509_STORE_INTERNAL_H
#define XL6_X509_STORE_INTERNAL_H
#include "xl6_x509_path_internal.h"

/* A published version is immutable while any snapshot or other store owns it.
 * The private mutex protects only this wrapper. It never spans language code
 * or external IO, and a live call retains its receiver through the ABI. */
typedef struct XL6_X509StoreVersionV1 {
    xatomic64 references;
    xx509store* value;
    int64_t bytes;
} XL6_X509StoreVersionV1;
typedef struct XL6_X509StoreV1 XL6_X509StoreV1;
#if !defined(XRT_SINGLE_HEADER) || defined(XRT_FEATURE_MUTEX)
struct XL6_X509StoreV1 {
    xmutex lock;
    bool initialized;
    XL6_X509StoreVersionV1* current;
};
#endif

XL6_X509StoreVersionV1* xl6X509StoreVersionTakeV1(xx509store**,int64_t);
XL6_X509StoreVersionV1* xl6X509StoreVersionRefV1(XL6_X509StoreVersionV1*);
void xl6X509StoreSnapshotDestroyV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509StoreSnapshotHandlePolicyV1Get(void);
XL6_X509StoreVersionV1* xl6X509StoreAcquireV1(void*);
void* xl6X509StoreCreateV1(void);
void xl6X509StoreDestroyV1(void*);
void* xl6X509StoreCloneV1(void*);
void* xl6X509StoreSnapshotV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509StoreHandlePolicyV1Get(void);
#endif
