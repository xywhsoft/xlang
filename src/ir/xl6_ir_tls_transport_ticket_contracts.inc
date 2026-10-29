#define TKO "xl6TlsStreamDestroyV1"
#define TKR "xl6TlsTicketResultDestroyV1"
#define TKE .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define TKC .nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1,.nativeClassResultOwner=TKR
#define TKS .nativeResultAbi=XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1,TKC
#define TKF XL6_IR_C_ABI_RUNTIME_FUTURE_PTR
{"xl6TlsStreamTicketCountV1",T,I,2,{V,I},.parameterOwners={TKO},TKE},
{"xl6TlsStreamDroppedTicketsV1",T,U,2,{V,I},.parameterOwners={TKO},TKE},
{"xl6TlsStreamTicketCountAsyncV1",T,TKF,1,{V},.parameterOwners={TKO},.nonblocking=1,.resultFutureElementAbi=I,TKE},
{"xl6TlsStreamDroppedTicketsAsyncV1",T,TKF,1,{V},.parameterOwners={TKO},.nonblocking=1,.resultFutureElementAbi=U,TKE},
{"xl6TlsStreamTryTakeTicketV1",T,0,1,{V},.parameterOwners={TKO},TKS,TKE},
{"xl6TlsStreamTryTakeTicketAsyncV1",T,TKF,1,{V},.parameterOwners={TKO},.nonblocking=1,.genericFutureResult=1,TKC,TKE},
{"xl6TlsStreamIssueNewTicketV1",T,0,1,{V},.parameterOwners={TKO},TKS,TKE},
{"xl6TlsStreamIssueTicketV1",T,0,4,{V,Y,I,I},.parameterOwners={TKO},TKS,TKE},
{"xl6TlsStreamIssueNewTicketAsyncV1",T,TKF,1,{V},.parameterOwners={TKO},.nonblocking=1,.genericFutureResult=1,TKC,TKE},
{"xl6TlsStreamIssueTicketAsyncV1",T,TKF,4,{V,Y,I,I},.parameterOwners={TKO},.nonblocking=1,.genericFutureResult=1,TKC,TKE},
#undef TKF
#undef TKS
#undef TKC
#undef TKE
#undef TKR
#undef TKO
