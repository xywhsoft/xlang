#define TST "xl6TlsStreamDestroyV1"
#define TSL "xl6TlsListenerDestroyV1"
#define TSE "xl6NetEngineDestroyV1"
#define TSA "xl6NetAddressDestroyV1"
#define TSC "xl6TlsClientContextDestroyV1"
#define TSS "xl6TlsServerContextDestroyV1"
#define TSO "xl6TlsStreamOptionsDestroyV1"
#define TSP "xl6TcpStreamOptionsDestroyV1"
#define TSN "xl6TcpListenOptionsDestroyV1"
#define TSF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define TSH .nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1
{"xl6TlsStreamCreateDefaultV1",T,V,5,{V,V,V,S,I},.parameterOwners={TSE,TSA,TSC},.resultOwner=TST,TSH,TSF},
{"xl6TlsStreamCreateV1",T,V,10,{V,V,V,S,I,S,I,V,V,I},.parameterOwners={TSE,TSA,TSC,NULL,NULL,NULL,NULL,TSO,TSP},.resultOwner=TST,TSH,TSF},
{"xl6TlsListenerCreateDefaultV1",T,V,3,{V,V,V},.parameterOwners={TSE,TSN,TSS},.resultOwner=TSL,TSH,TSF},
{"xl6TlsListenerCreateV1",T,V,6,{V,V,V,V,I,I},.parameterOwners={TSE,TSN,TSS,TSO},.resultOwner=TSL,TSH,TSF},
{"xl6TlsStreamDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TST},.nativeHandlePolicy="xl6TlsStreamHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsListenerDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TSL},.nativeHandlePolicy="xl6TlsListenerHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsStreamCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TST},.nonblocking=1,TSF},
{"xl6TlsStreamAbortV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TST},.nonblocking=1,TSF},
{"xl6TlsListenerCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TSL},.nonblocking=1,TSF},
{"xl6TlsStreamStateV1",T,I,1,{V},.parameterOwners={TST},.nonblocking=1,TSF},
{"xl6TlsListenerStateV1",T,I,1,{V},.parameterOwners={TSL},.nonblocking=1,TSF},
{"xl6TlsStreamAvailableV1",T,U,1,{V},.parameterOwners={TST},.nonblocking=1,TSF},
{"xl6TlsStreamPendingV1",T,U,1,{V},.parameterOwners={TST},.nonblocking=1,TSF},
{"xl6TlsStreamLocalV1",T,V,1,{V},.parameterOwners={TST},.resultOwner=TSA,.nonblocking=1,TSF},
{"xl6TlsStreamRemoteV1",T,V,1,{V},.parameterOwners={TST},.resultOwner=TSA,.nonblocking=1,TSF},
{"xl6TlsListenerLocalV1",T,V,1,{V},.parameterOwners={TSL},.resultOwner=TSA,.nonblocking=1,TSF},
#undef TST
#undef TSL
#undef TSE
#undef TSA
#undef TSC
#undef TSS
#undef TSO
#undef TSP
#undef TSN
#undef TSF
#undef TSH
