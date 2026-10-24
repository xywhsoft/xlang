#define TO "xl6TlsStreamOptionsDestroyV1"
#define TE .nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define TG .parameterOwners={TO},.pure=1,TE
{"xl6TlsStreamOptionsCreateV1",T,V,0,{0},.resultOwner=TO,TE},
{"xl6TlsStreamOptionsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TO},.nativeHandlePolicy="xl6TlsStreamOptionsHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsStreamOptionsWithTimeoutsV1",T,V,3,{V,I,I},.parameterOwners={TO},.resultOwner=TO,TE},
{"xl6TlsStreamOptionsWithAsyncLimitsV1",T,V,4,{V,U,I,I},.parameterOwners={TO},.resultOwner=TO,TE},
#define XL6_TLS_STREAM_MILLIS(Field,Name) {"xl6TlsStreamOptions" #Field "V1",T,I,1,{V},TG},
#define XL6_TLS_STREAM_BYTES(Field,Name) {"xl6TlsStreamOptions" #Field "V1",T,U,1,{V},TG},
#define XL6_TLS_STREAM_COUNT(Field,Name) {"xl6TlsStreamOptions" #Field "V1",T,I,1,{V},TG},
#include "src/runtime/xl6_tls_stream_options_fields.inc"
#undef XL6_TLS_STREAM_MILLIS
#undef XL6_TLS_STREAM_BYTES
#undef XL6_TLS_STREAM_COUNT
#undef TG
#undef TE
#undef TO
