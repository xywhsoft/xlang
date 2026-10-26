const char* xl6StdlibTlsMetadataSource=
#define XL6_TLS_ENUM_BEGIN(Type,Id,Count) "enum " #Type " as int\n"
#define XL6_TLS_ENUM_ITEM(Name,Value) "    " #Name " = " #Value "\n"
#define XL6_TLS_ENUM_END(Type,Id,Count) "end\n"
#include "src/abi/xl6_tls_transport_enums.inc"
#undef XL6_TLS_ENUM_BEGIN
#undef XL6_TLS_ENUM_ITEM
#undef XL6_TLS_ENUM_END
    "class TlsConnectionInfo\n    private native point handle\n"
    "    extern \"xl6TlsInfoDestroyV1\" @no_raise @nonblocking destroy()\n"
#define XL6_TLS_INFO_ENUM(Field,Name,Type) "    extern \"xl6TlsInfo" #Field "V1\" @pure @nonblocking get " #Name "() as " #Type "\n"
#define XL6_TLS_INFO_INT(Field,Name) XL6_TLS_INFO_ENUM(Field,Name,int)
#define XL6_TLS_INFO_UINT(Field,Name) XL6_TLS_INFO_ENUM(Field,Name,uint64)
#define XL6_TLS_INFO_BOOL(Field,Name) XL6_TLS_INFO_ENUM(Field,Name,bool)
#define XL6_TLS_INFO_BYTES(Field,Name) XL6_TLS_INFO_ENUM(Field,Name,bytes)
#define XL6_TLS_INFO_TEXT(Field,Name) XL6_TLS_INFO_ENUM(Field,Name,string)
#define XL6_TLS_STATS_UINT(Field,Name)
#include "src/runtime/xl6_tls_metadata_fields.h"
XL6_TLS_METADATA_FIELDS()
#undef XL6_TLS_INFO_ENUM
#undef XL6_TLS_INFO_INT
#undef XL6_TLS_INFO_UINT
#undef XL6_TLS_INFO_BOOL
#undef XL6_TLS_INFO_BYTES
#undef XL6_TLS_INFO_TEXT
#undef XL6_TLS_STATS_UINT
    "    extern \"xl6TlsInfoVersionV1\" @pure @nonblocking get version() as TlsVersion\n"
    "    extern \"xl6TlsInfoCipherV1\" @pure @nonblocking get cipher() as TlsCipherSuite\n"
    "    extern \"xl6TlsInfoCertificateCountV1\" @pure @nonblocking get peerCertificateCount() as int\n"
    "    extern \"xl6TlsInfoCertificateAtV1\" @pure @nonblocking func peerCertificateAt(int index = 0) as Certificate\n"
    "end\n"
    "class TlsServerStats\n    private native point handle\n"
    "    extern \"xl6TlsServerStatsDestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6TlsServerStatsStateV1\" @pure @nonblocking get state() as TlsListenerState\n"
#define XL6_TLS_INFO_ENUM(Field,Name,Type)
#define XL6_TLS_INFO_INT(Field,Name)
#define XL6_TLS_INFO_UINT(Field,Name)
#define XL6_TLS_INFO_BOOL(Field,Name)
#define XL6_TLS_INFO_BYTES(Field,Name)
#define XL6_TLS_INFO_TEXT(Field,Name)
#define XL6_TLS_STATS_UINT(Field,Name) "    extern \"xl6TlsServerStats" #Field "V1\" @pure @nonblocking get " #Name "() as uint64\n"
#include "src/runtime/xl6_tls_metadata_fields.h"
XL6_TLS_METADATA_FIELDS()
#undef XL6_TLS_INFO_ENUM
#undef XL6_TLS_INFO_INT
#undef XL6_TLS_INFO_UINT
#undef XL6_TLS_INFO_BOOL
#undef XL6_TLS_INFO_BYTES
#undef XL6_TLS_INFO_TEXT
#undef XL6_TLS_STATS_UINT
    "end\n";
