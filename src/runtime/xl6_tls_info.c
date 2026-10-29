#include "xl6_tls_metadata_internal.h"
#include "xl6_tls_transport_internal.h"

const XL6_TlsInfoV1* xl6TlsInfoCheckedV1(const void* raw)
{
    if(!raw){xrtSetErrorKind(XERR_ARGUMENT);xl6TlsTransportFailedV1();}
    return raw;
}
XL6_TlsInfoV1* xl6TlsInfoRefV1(XL6_TlsInfoV1* value)
{return value&&xrtRefRetain(&value->references)>=0?value:NULL;}
void xl6TlsInfoDestroyV1(void* raw)
{
    XL6_TlsInfoV1* value=raw;
    if(value&&xrtRefRelease(&value->references)==0)xrtFree(value);
}
#define XL6_TLS_INFO_ENUM(Field,Name,Type) XL6_TLS_INFO_INT(Field,Name)
#define XL6_TLS_INFO_INT(Field,Name) \
int64_t xl6TlsInfo##Field##V1(const void* raw) \
{const XL6_TlsInfoV1* v=xl6TlsInfoCheckedV1(raw);return v?v->Name:0;}
#define XL6_TLS_INFO_UINT(Field,Name) \
uint64_t xl6TlsInfo##Field##V1(const void* raw) \
{const XL6_TlsInfoV1* v=xl6TlsInfoCheckedV1(raw);return v?v->Name:0;}
#define XL6_TLS_INFO_BOOL(Field,Name) \
bool xl6TlsInfo##Field##V1(const void* raw) \
{const XL6_TlsInfoV1* v=xl6TlsInfoCheckedV1(raw);return v&&v->Name;}
#define XL6_TLS_INFO_BYTES(Field,Name) \
xvalue* xl6TlsInfo##Field##V1(const void* raw) \
{const XL6_TlsInfoV1* v=xl6TlsInfoCheckedV1(raw);return v?xrtValueBytes(v->Name):NULL;}
#define XL6_TLS_INFO_TEXT(Field,Name) \
xstrview xl6TlsInfo##Field##V1(const void* raw) { \
    const XL6_TlsInfoV1* v=xl6TlsInfoCheckedV1(raw);if(!v)return (xstrview){0}; \
    char* text=xrtMalloc(v->Name.Size+1);if(!text)return (xstrview){0}; \
    if(v->Name.Size){memcpy(text,v->Name.Data,v->Name.Size);} \
    text[v->Name.Size]=0; \
    return (xstrview){text,v->Name.Size}; }
#define XL6_TLS_STATS_UINT(Field,Name)
#include "xl6_tls_metadata_fields.h"
XL6_TLS_METADATA_FIELDS()
#undef XL6_TLS_INFO_ENUM
#undef XL6_TLS_INFO_INT
#undef XL6_TLS_INFO_UINT
#undef XL6_TLS_INFO_BOOL
#undef XL6_TLS_INFO_BYTES
#undef XL6_TLS_INFO_TEXT
#undef XL6_TLS_STATS_UINT
static int64_t selection(const void* raw,bool cipher)
{
    const XL6_TlsInfoV1* v=xl6TlsInfoCheckedV1(raw);
    if(!v)return 0;
    int64_t result=cipher?v->cipher:v->version;
    if(!result){xrtSetErrorKind(XERR_STATE);xl6TlsTransportFailedV1();}
    return result;
}
int64_t xl6TlsInfoVersionV1(const void* raw){return selection(raw,false);}
int64_t xl6TlsInfoCipherV1(const void* raw){return selection(raw,true);}
int64_t xl6TlsInfoCertificateCountV1(const void* raw)
{const XL6_TlsInfoV1* v=xl6TlsInfoCheckedV1(raw);return v?(int64_t)v->certificateCount:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TlsInfoHandlePolicyV1Get,xl6TlsInfoDestroyV1)
