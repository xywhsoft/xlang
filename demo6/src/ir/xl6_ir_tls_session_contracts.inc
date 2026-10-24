#define SD "xl6TlsSessionDestroyV1"
#define SE .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define SF .nonblocking=1,SE
#define SS .parameterOwners={SD}
#define SR .resultEnum=&xl6IrTlsResultContract
#define SW .resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64U64_V1,.resultProductEnums={&xl6IrTlsResultContract}
{"xl6TlsSessionClientCreateV1",T,V,5,{V,S,I,S,I},.parameterOwners={"xl6TlsClientContextDestroyV1"},.resultOwner=SD,SE},
{"xl6TlsSessionServerCreateV1",T,V,1,{V},.parameterOwners={"xl6TlsServerContextDestroyV1"},.resultOwner=SD,SE},
{"xl6TlsSessionDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},SS,.nativeHandlePolicy="xl6TlsSessionHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsSessionDisposeV1",T,XL6_IR_C_ABI_VOID,1,{V},SS,SF},
{"xl6TlsSessionDisposedV1",T,B,1,{V},SS,SF},
{"xl6TlsSessionRoleV1",T,I,1,{V},SS,.resultEnum=&xl6IrTlsRoleContract,SF},
{"xl6TlsSessionStateV1",T,I,1,{V},SS,.resultEnum=&xl6IrTlsStateContract,SF},
{"xl6TlsSessionWaitV1",T,I,1,{V},SS,.resultEnum=&xl6IrTlsWaitContract,SF},
{"xl6TlsSessionVersionV1",T,I,1,{V},SS,.resultEnum=&xl6IrTlsVersionContract,SF},
{"xl6TlsSessionCipherV1",T,I,1,{V},SS,.resultEnum=&xl6IrTlsCipherSuiteContract,SF},
{"xl6TlsSessionReadyV1",T,B,1,{V},SS,SF},
{"xl6TlsSessionPendingInputV1",T,U,1,{V},SS,SF},
{"xl6TlsSessionPendingCipherV1",T,U,1,{V},SS,SF},
{"xl6TlsSessionPendingPlainV1",T,U,1,{V},SS,SF},
{"xl6TlsSessionDriveV1",T,I,1,{V},SS,SR,SE},
{"xl6TlsSessionCloseV1",T,I,1,{V},SS,SR,SF},
{"xl6TlsSessionEofV1",T,I,1,{V},SS,SR,SF},
{"xl6TlsSessionFeedTextV1",T,I,3,{V,S,I},SS,SR,SF},
{"xl6TlsSessionFeedBytesV1",T,I,3,{V,Y,I},SS,SR,SF},
{"xl6TlsSessionFeedBufferV1",T,I,2,{V,V},.parameterOwners={SD,"xl6NetBufferDestroyV1"},SR,SF},
{"xl6TlsSessionWriteTextV1",T,XL6_IR_C_ABI_PRODUCT,3,{V,S,I},SS,SW,SF},
{"xl6TlsSessionWriteBytesV1",T,XL6_IR_C_ABI_PRODUCT,3,{V,Y,I},SS,SW,SF},
{"xl6TlsSessionReadV1",T,XL6_IR_C_ABI_PRODUCT,2,{V,I},SS,.resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64BYTES_V1,.resultProductEnums={&xl6IrTlsResultContract},SF},
{"xl6TlsSessionTakeCipherV1",T,Y,2,{V,I},SS,SF},
{"xl6TlsSessionProtocolV1",T,Y,1,{V},SS,SF},
#undef SW
#undef SR
#undef SS
#undef SF
#undef SE
#undef SD
