#include "xl6_x509_store_internal.h"
static const XL6_X509StoreVersionV1* checked(void* raw)
{if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing trust store snapshot");return raw;}
int64_t xl6X509StoreSnapshotCountV1(void* raw)
{const XL6_X509StoreVersionV1* self=checked(raw);return self?(int64_t)xrtX509StoreCount(self->value):0;}
int64_t xl6X509StoreSnapshotBytesV1(void* raw)
{const XL6_X509StoreVersionV1* self=checked(raw);return self?self->bytes:0;}
