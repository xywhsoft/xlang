const char* xl6StdlibTlsStreamOptionsSource=
    "class TlsStreamOptions\n"
    "    private native point handle\n"
    "    extern \"xl6TlsStreamOptionsCreateV1\" @nonblocking create()\n"
    "    extern \"xl6TlsStreamOptionsDestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6TlsStreamOptionsWithTimeoutsV1\" @nonblocking func withTimeoutsMillis(int handshakeTimeoutMillis, int closeTimeoutMillis) as TlsStreamOptions\n"
    "    extern \"xl6TlsStreamOptionsWithAsyncLimitsV1\" @nonblocking func withAsyncLimits(uint64 maxBytes, int maxOperations, int batch = 64) as TlsStreamOptions\n"
#define XL6_TLS_STREAM_MILLIS(Field,Name) "    extern \"xl6TlsStreamOptions" #Field "V1\" @pure @nonblocking get " #Name "() as int\n"
#define XL6_TLS_STREAM_BYTES(Field,Name) "    extern \"xl6TlsStreamOptions" #Field "V1\" @pure @nonblocking get " #Name "() as uint64\n"
#define XL6_TLS_STREAM_COUNT(Field,Name) "    extern \"xl6TlsStreamOptions" #Field "V1\" @pure @nonblocking get " #Name "() as int\n"
#include "src/runtime/xl6_tls_stream_options_fields.inc"
#undef XL6_TLS_STREAM_MILLIS
#undef XL6_TLS_STREAM_BYTES
#undef XL6_TLS_STREAM_COUNT
    "end\n";
