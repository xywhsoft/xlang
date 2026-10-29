#include "xl6_x509_store_internal.h"
int64_t xl6X509StoreCountV1(void* raw)
{
    XL6_X509StoreVersionV1* version=xl6X509StoreAcquireV1(raw);
    int64_t result=version?xl6X509StoreSnapshotCountV1(version):0;
    xl6X509StoreSnapshotDestroyV1(version);return result;
}
int64_t xl6X509StoreBytesV1(void* raw)
{
    XL6_X509StoreVersionV1* version=xl6X509StoreAcquireV1(raw);
    int64_t result=version?xl6X509StoreSnapshotBytesV1(version):0;
    xl6X509StoreSnapshotDestroyV1(version);return result;
}
