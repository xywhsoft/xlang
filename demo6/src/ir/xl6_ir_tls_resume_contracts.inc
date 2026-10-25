#define TR "xl6TlsResumeDestroyV1"
#define TRG .pure=1,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define TRP .parameterOwners={TR},TRG
{"xl6TlsResumeFromPartsV1",T,V,11,{I,I,Y,Y,M,I,U,S,Y,Y,U},.parameterEnums={&xl6IrTlsVersionContract,&xl6IrTlsCipherSuiteContract},.resultOwner=TR,TRG},
{"xl6TlsResumeDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TR},.nativeHandlePolicy="xl6TlsResumeHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsResumeVersionV1",T,I,1,{V},.resultEnum=&xl6IrTlsVersionContract,TRP},
{"xl6TlsResumeCipherV1",T,I,1,{V},.resultEnum=&xl6IrTlsCipherSuiteContract,TRP},
{"xl6TlsResumeTicketV1",T,Y,1,{V},TRP},
{"xl6TlsResumeExportSecretV1",T,Y,1,{V},TRP},
{"xl6TlsResumeServerNameV1",T,S,1,{V},TRP},
{"xl6TlsResumeProtocolV1",T,Y,1,{V},TRP},
{"xl6TlsResumePeerIdentityV1",T,Y,1,{V},TRP},
{"xl6TlsResumeLifetimeV1",T,I,1,{V},TRP},
{"xl6TlsResumeAgeAddV1",T,U,1,{V},TRP},
{"xl6TlsResumeMaxEarlyDataV1",T,U,1,{V},TRP},
{"xl6TlsResumeIssuedAtV1",T,M,1,{V},TRP},
{"xl6TlsResumeExpiresAtV1",T,M,1,{V},TRP},
{"xl6TlsResumeValidAtV1",T,B,2,{V,M},TRP},
{"xl6TlsResumeTicketAgeV1",T,U,2,{V,M},TRP},
#undef TRP
#undef TRG
#undef TR
