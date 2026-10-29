#include "xl6_tls_metadata_internal.h"
#include "xl6_tls_transport_internal.h"
static const xtlslistenerstats* checked(const void* raw)
{
    if(!raw){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();}
    return raw;
}
void xl6TlsServerStatsDestroyV1(void* raw){xrtFree(raw);}
int64_t xl6TlsServerStatsStateV1(const void* raw)
{const xtlslistenerstats* value=checked(raw);return value?value->State:0;}
#define XL6_TLS_INFO_ENUM(Field,Name,Type)
#define XL6_TLS_INFO_INT(Field,Name)
#define XL6_TLS_INFO_UINT(Field,Name)
#define XL6_TLS_INFO_BOOL(Field,Name)
#define XL6_TLS_INFO_BYTES(Field,Name)
#define XL6_TLS_INFO_TEXT(Field,Name)
#define XL6_TLS_STATS_UINT(Field,Name) \
uint64_t xl6TlsServerStats##Field##V1(const void* raw) \
{const xtlslistenerstats* value=checked(raw);return value?value->Field:0;}
#include "xl6_tls_metadata_fields.h"
XL6_TLS_METADATA_FIELDS()
#undef XL6_TLS_INFO_ENUM
#undef XL6_TLS_INFO_INT
#undef XL6_TLS_INFO_UINT
#undef XL6_TLS_INFO_BOOL
#undef XL6_TLS_INFO_BYTES
#undef XL6_TLS_INFO_TEXT
#undef XL6_TLS_STATS_UINT
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsServerStatsHandlePolicyV1Get,xl6TlsServerStatsDestroyV1)
