#define TTR "xl6TlsTicketResultDestroyV1"
#define TTS "xl6TlsSessionDestroyV1"
#define TTE .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define TTC .nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_RESULT_V1,.nativeClassResultOwner=TTR,.nativeResultAbi=XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1
{"xl6TlsTicketResultDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TTR},.nativeHandlePolicy="xl6TlsTicketResultHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsTicketResultStatusV1",T,I,1,{V},.parameterOwners={TTR},.resultEnum=&xl6IrTlsResultContract,.pure=1,.nonblocking=1,TTE},
{"xl6TlsTicketResultResumeV1",T,V,1,{V},.parameterOwners={TTR},.resultOwner="xl6TlsResumeDestroyV1",.nullableResult=1,.pure=1,.nonblocking=1,TTE},
{"xl6TlsSessionTicketCountV1",T,I,1,{V},.parameterOwners={TTS},.nonblocking=1,TTE},
{"xl6TlsSessionDroppedTicketsV1",T,U,1,{V},.parameterOwners={TTS},.nonblocking=1,TTE},
{"xl6TlsSessionTryTakeTicketV1",T,0,1,{V},.parameterOwners={TTS},.nonblocking=1,TTC,TTE},
{"xl6TlsSessionIssueNewTicketV1",T,0,1,{V},.parameterOwners={TTS},TTC,TTE},
{"xl6TlsSessionIssueTicketV1",T,0,4,{V,Y,I,I},.parameterOwners={TTS},TTC,TTE},
#undef TTC
#undef TTE
#undef TTS
#undef TTR
