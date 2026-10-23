#include "xl6_x509_store_internal.h"
void* xl6X509StoreCertificateV1(void* raw,int64_t index)
{
    XL6_X509StoreVersionV1* version=xl6X509StoreAcquireV1(raw);
    void* result=version?xl6X509StoreSnapshotCertificateV1(version,index):NULL;
    xl6X509StoreSnapshotDestroyV1(version);return result;
}
void* xl6X509StoreAnchorV1(void* raw,int64_t index)
{
    XL6_X509StoreVersionV1* version=xl6X509StoreAcquireV1(raw);
    void* result=version?xl6X509StoreSnapshotAnchorV1(version,index):NULL;
    xl6X509StoreSnapshotDestroyV1(version);return result;
}
