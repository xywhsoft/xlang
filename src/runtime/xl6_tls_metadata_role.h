#ifndef XL6_TLS_METADATA_ROLE_H
#define XL6_TLS_METADATA_ROLE_H
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "include/xl_clock.h"
/* Permanent resident role operations, installed by the actual creator. A generic
 * Stream/metadata consumer must not link both handshake implementations just
 * to inspect a role. No language/module callback or owned edge is stored here. */
typedef struct XL6_TlsMetadataRoleV1 {
    xtlsrole role;
    bool (*Resumed)(const xtlssession*);
    size_t (*CertificateCount)(const xtlssession*);
    const xx509cert* (*CertificateAt)(const xtlssession*,size_t);
    bool (*ServerName)(const xtlssession*,xbytesview*);
    bool (*ServerCookie)(const xtlssession*,uint64*);
    /* Mutable control is used only under the actual owning Worker scope. */
    xtlsresult (*KeyUpdate)(xtlssession*,xtlskeyupdate);
} XL6_TlsMetadataRoleV1;
const XL6_TlsMetadataRoleV1* xl6TlsClientMetadataRoleV1Get(void);
const XL6_TlsMetadataRoleV1* xl6TlsServerMetadataRoleV1Get(void);
#endif
