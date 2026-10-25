#ifndef XL6_TLS_METADATA_INTERNAL_H
#define XL6_TLS_METADATA_INTERNAL_H
#include "xl6_tls_metadata_abi.h"
#include "xl6_native_handle.h"
#include "xl6_tls_metadata_role.h"

/* One immutable allocation. Every view points inside this allocation, never
 * into a Session, transport, Error, language object or module descriptor.
 * A private atomic ref is used only to transfer a synchronous Task result. */
typedef struct XL6_TlsInfoV1 {
    volatile int32_t references;
#define XL6_TLS_INFO_ENUM(Field,Name,Type) int64_t Name;
#define XL6_TLS_INFO_INT(Field,Name) int64_t Name;
#define XL6_TLS_INFO_UINT(Field,Name) uint64_t Name;
#define XL6_TLS_INFO_BOOL(Field,Name) bool Name;
#define XL6_TLS_INFO_BYTES(Field,Name) xbytesview Name;
#define XL6_TLS_INFO_TEXT(Field,Name) xstrview Name;
#define XL6_TLS_STATS_UINT(Field,Name)
#include "xl6_tls_metadata_fields.inc"
#undef XL6_TLS_INFO_ENUM
#undef XL6_TLS_INFO_INT
#undef XL6_TLS_INFO_UINT
#undef XL6_TLS_INFO_BOOL
#undef XL6_TLS_INFO_BYTES
#undef XL6_TLS_INFO_TEXT
#undef XL6_TLS_STATS_UINT
    int64_t version,cipher;
    size_t certificateCount;
    xbytesview certificates[];
} XL6_TlsInfoV1;

const XL6_TlsInfoV1* xl6TlsInfoCheckedV1(const void*);
XL6_TlsInfoV1* xl6TlsInfoRefV1(XL6_TlsInfoV1*);
struct xtlsstream;
XL6_TlsInfoV1* xl6TlsInfoBuildV1(struct xtlsstream*,const XL6_TlsMetadataRoleV1*);
struct xfuture* xl6TlsInfoSourceV1(void*,const XL6_NativeClassResultContextV1*);
const XL6_NativeHandlePolicyV1* xl6TlsInfoHandlePolicyV1Get(void);
const XL6_NativeHandlePolicyV1* xl6TlsServerStatsHandlePolicyV1Get(void);
#endif
