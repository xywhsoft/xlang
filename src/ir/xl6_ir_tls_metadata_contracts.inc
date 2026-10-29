#define TMI "xl6TlsInfoDestroyV1"
#define TMS "xl6TlsServerStatsDestroyV1"
#define TMF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define TMG(Owner) .parameterOwners={Owner},.pure=1,.nonblocking=1,TMF
{"xl6TlsStreamSnapshotV1",T,V,2,{V,I},.parameterOwners={"xl6TlsStreamDestroyV1"},.resultOwner=TMI,TMF},
{"xl6TlsStreamSnapshotAsyncV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,1,{V},.parameterOwners={"xl6TlsStreamDestroyV1"},.nonblocking=1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1,.genericFutureResult=1,.nativeClassResultOwner=TMI,TMF},
{"xl6TlsListenerSnapshotV1",T,V,1,{V},.parameterOwners={"xl6TlsListenerDestroyV1"},.resultOwner=TMS,.nonblocking=1,TMF},
{"xl6TlsInfoDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TMI},.nativeHandlePolicy="xl6TlsInfoHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsServerStatsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TMS},.nativeHandlePolicy="xl6TlsServerStatsHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsInfoVersionV1",T,I,1,{V},TMG(TMI),.resultEnum=&xl6IrTlsVersionContract},
{"xl6TlsInfoCipherV1",T,I,1,{V},TMG(TMI),.resultEnum=&xl6IrTlsCipherSuiteContract},
{"xl6TlsInfoCertificateCountV1",T,I,1,{V},TMG(TMI)},
{"xl6TlsInfoCertificateAtV1",T,V,2,{V,I},.parameterOwners={TMI},.resultOwner="xl6X509CertificateDestroyV1",.pure=1,.nonblocking=1,TMF},
{"xl6TlsServerStatsStateV1",T,I,1,{V},TMG(TMS),.resultEnum=&xl6IrTlsListenerStateContract},
#define XL6_TLS_INFO_ENUM(Field,Name,Type) {"xl6TlsInfo" #Field "V1",T,I,1,{V},TMG(TMI),.resultEnum=&xl6Ir##Type##Contract},
#define XL6_TLS_INFO_INT(Field,Name) {"xl6TlsInfo" #Field "V1",T,I,1,{V},TMG(TMI)},
#define XL6_TLS_INFO_UINT(Field,Name) {"xl6TlsInfo" #Field "V1",T,U,1,{V},TMG(TMI)},
#define XL6_TLS_INFO_BOOL(Field,Name) {"xl6TlsInfo" #Field "V1",T,B,1,{V},TMG(TMI)},
#define XL6_TLS_INFO_BYTES(Field,Name) {"xl6TlsInfo" #Field "V1",T,Y,1,{V},TMG(TMI)},
#define XL6_TLS_INFO_TEXT(Field,Name) {"xl6TlsInfo" #Field "V1",T,S,1,{V},TMG(TMI)},
#define XL6_TLS_STATS_UINT(Field,Name) {"xl6TlsServerStats" #Field "V1",T,U,1,{V},TMG(TMS)},
#include "src/runtime/xl6_tls_metadata_fields.h"
XL6_TLS_METADATA_FIELDS()
#undef XL6_TLS_INFO_ENUM
#undef XL6_TLS_INFO_INT
#undef XL6_TLS_INFO_UINT
#undef XL6_TLS_INFO_BOOL
#undef XL6_TLS_INFO_BYTES
#undef XL6_TLS_INFO_TEXT
#undef XL6_TLS_STATS_UINT
#undef TMI
#undef TMS
#undef TMF
#undef TMG
