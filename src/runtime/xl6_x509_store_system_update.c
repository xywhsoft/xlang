#include "xl6_x509_store_internal.h"

void* xl6X509StoreSystemV1(void)
{
    return xl6X509StoreValueTakeV1(xl6X509StoreSnapshotSystemV1());
}
int64_t xl6X509StoreAddSystemV1(void* raw)
{
    if(xrtGetError())return 0;
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing trust store");return 0;}
    /* Native OS enumeration and parsing never run under the destination lock. */
    void* source=xl6X509StoreSnapshotSystemV1();if(!source)return 0;
    int64_t added=xl6X509StoreMergeV1(raw,source);
    xl6X509StoreSnapshotDestroyV1(source);return added;
}
