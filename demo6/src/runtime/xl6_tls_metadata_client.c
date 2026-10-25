#include "xl6_tls_metadata_role.h"
const XL6_TlsMetadataRoleV1* xl6TlsClientMetadataRoleV1Get(void)
{
    static const XL6_TlsMetadataRoleV1 role={XTLS_CLIENT,xrtTlsClientResumed,
        xrtTlsClientCertificateCount,xrtTlsClientCertificate,NULL,NULL};
    return &role;
}
