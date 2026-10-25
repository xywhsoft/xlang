#define RR "xl6TlsResumeRequestDestroyV1"
#define RS "xl6TlsServerContextDestroyV1"
#define RG .pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6TlsResumeRequestDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RR},.nativeHandlePolicy="xl6TlsResumeRequestHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsResumeRequestNameV1",T,Y,1,{V},.parameterOwners={RR},RG},
{"xl6TlsResumeRequestProtocolCountV1",T,I,1,{V},.parameterOwners={RR},RG},
{"xl6TlsResumeRequestProtocolAtV1",T,Y,2,{V,I},.parameterOwners={RR},RG},
{"xl6TlsResumeRequestTicketV1",T,Y,1,{V},.parameterOwners={RR},RG},
{"xl6TlsResumeRequestAgeV1",T,U,1,{V},.parameterOwners={RR},RG},
{"xl6TlsServerContextWithResumeLookupV1",T,V,3,{V,XL6_IR_C_ABI_CLOSURE_PTR,U},.parameterOwners={RS},.resultOwner=RS,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_CALLBACK_V1,.nativeClassCallbackParameter=2,.nativeClassCallbackArgumentOwner=RR,.nativeClassCallbackResultOwner="xl6TlsResumeDestroyV1",RG},
{"xl6TlsServerContextWithoutResumeLookupV1",T,V,1,{V},.parameterOwners={RS},.resultOwner=RS,RG},
{"xl6TlsServerContextHasResumeLookupV1",T,B,1,{V},.parameterOwners={RS},RG},
{"xl6TlsServerContextResumeAgeToleranceV1",T,U,1,{V},.parameterOwners={RS},RG},
#undef RG
#undef RS
#undef RR
