#include "xl6_tls_metadata_role.h"
const XL6_TlsMetadataRoleV1* xl6TlsServerMetadataRoleV1Get(void)
{
    static const XL6_TlsMetadataRoleV1 role={XTLS_SERVER,xrtTlsServerResumed,
        NULL,NULL,xrtTlsServerName,xrtTlsServerCookie,xrtTlsServerKeyUpdate};
    return &role;
}
