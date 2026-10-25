#define TC "xl6TlsClientContextDestroyV1"
#define TS "xl6TlsServerContextDestroyV1"
#define TP "xl6TlsParametersDestroyV1"
#define TV "xl6TlsVerifierDestroyV1"
#define TI "xl6TlsIdentityDestroyV1"
#define TG .pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6TlsClientContextDefaultV1",T,V,1,{V},.parameterOwners={TV},.resultOwner=TC,TG},
{"xl6TlsClientContextCreateV1",T,V,2,{V,V},.parameterOwners={TP,TV},.resultOwner=TC,TG},
{"xl6TlsClientContextDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC},.nativeHandlePolicy="xl6TlsClientContextHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsClientContextParametersV1",T,V,1,{V},.parameterOwners={TC},.resultOwner=TP,TG},
{"xl6TlsClientContextVerifierV1",T,V,1,{V},.parameterOwners={TC},.resultOwner=TV,TG},
{"xl6TlsClientContextWithParametersV1",T,V,2,{V,V},.parameterOwners={TC,TP},.resultOwner=TC,TG},
{"xl6TlsClientContextWithVerifierV1",T,V,2,{V,V},.parameterOwners={TC,TV},.resultOwner=TC,TG},
{"xl6TlsClientContextWithResumeV1",T,V,2,{V,V},.parameterOwners={TC,"xl6TlsResumeDestroyV1"},.resultOwner=TC,TG},
{"xl6TlsClientContextWithoutResumeV1",T,V,1,{V},.parameterOwners={TC},.resultOwner=TC,TG},
{"xl6TlsClientContextHasResumeV1",T,B,1,{V},.parameterOwners={TC},TG},
{"xl6TlsClientContextResumeV1",T,V,1,{V},.parameterOwners={TC},.resultOwner="xl6TlsResumeDestroyV1",.nullableResult=1,TG},
{"xl6TlsClientContextWithRequiredResumeV1",T,V,2,{V,V},.parameterOwners={TC,"xl6TlsResumeDestroyV1"},.resultOwner=TC,TG},
{"xl6TlsClientContextWithTicketCapacityV1",T,V,2,{V,I},.parameterOwners={TC},.resultOwner=TC,TG},
{"xl6TlsClientContextTicketCapacityV1",T,I,1,{V},.parameterOwners={TC},TG},
{"xl6TlsClientContextResumeRequiredV1",T,B,1,{V},.parameterOwners={TC},TG},
{"xl6TlsClientContextProtocolCountV1",T,I,1,{V},.parameterOwners={TC},TG},
{"xl6TlsClientContextProtocolAtV1",T,Y,2,{V,I},.parameterOwners={TC},TG},
{"xl6TlsServerContextDefaultV1",T,V,1,{V},.parameterOwners={TI},.resultOwner=TS,TG},
{"xl6TlsServerContextCreateV1",T,V,2,{V,V},.parameterOwners={TP,TI},.resultOwner=TS,TG},
{"xl6TlsServerContextDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TS},.nativeHandlePolicy="xl6TlsServerContextHandlePolicyV1Get",.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6TlsServerContextParametersV1",T,V,1,{V},.parameterOwners={TS},.resultOwner=TP,TG},
{"xl6TlsServerContextIdentityV1",T,V,1,{V},.parameterOwners={TS},.resultOwner=TI,TG},
{"xl6TlsServerContextWithParametersV1",T,V,2,{V,V},.parameterOwners={TS,TP},.resultOwner=TS,TG},
{"xl6TlsServerContextWithIdentityV1",T,V,2,{V,V},.parameterOwners={TS,TI},.resultOwner=TS,TG},
{"xl6TlsServerContextProtocolCountV1",T,I,1,{V},.parameterOwners={TS},TG},
{"xl6TlsServerContextProtocolAtV1",T,Y,2,{V,I},.parameterOwners={TS},TG},
{"xl6TlsServerContextRequiresProtocolV1",T,B,1,{V},.parameterOwners={TS},TG},
#undef TG
#undef TI
#undef TV
#undef TP
#undef TS
#undef TC
