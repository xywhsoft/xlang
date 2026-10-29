#define QC "xl6TlsServerChoiceDestroyV1"
#define QR "xl6TlsServerRequestDestroyV1"
#define QS "xl6TlsServerContextDestroyV1"
#define QF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define QP .pure=1,.nonblocking=1,QF
#define QCB .nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_CALLBACK_V1,.nativeClassCallbackParameter=2,.nativeClassCallbackArgumentOwner=QR,.nativeClassCallbackResultOwner=QC
{"xl6TlsServerRequestDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={QR},.nativeHandlePolicy="xl6TlsServerRequestHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsServerRequestNameV1",T,Y,1,{V},.parameterOwners={QR},QP},
{"xl6TlsServerRequestProtocolCountV1",T,I,1,{V},.parameterOwners={QR},QP},
{"xl6TlsServerRequestProtocolAtV1",T,Y,2,{V,I},.parameterOwners={QR},QP},
{"xl6TlsServerChoiceCreateV1",T,V,2,{V,U},.parameterOwners={"xl6TlsIdentityDestroyV1"},.resultOwner=QC,QP},
{"xl6TlsServerChoiceDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={QC},.nativeHandlePolicy="xl6TlsServerChoiceHandlePolicyV1Get",.noRaise=1,.nonblocking=1},
{"xl6TlsServerChoiceWithProtocolIndexV1",T,V,2,{V,I},.parameterOwners={QC},.resultOwner=QC,QP},
{"xl6TlsServerChoiceIdentityV1",T,V,1,{V},.parameterOwners={QC},.resultOwner="xl6TlsIdentityDestroyV1",QP},
{"xl6TlsServerChoiceCookieV1",T,U,1,{V},.parameterOwners={QC},QP},
{"xl6TlsServerChoiceHasProtocolIndexV1",T,B,1,{V},.parameterOwners={QC},QP},
{"xl6TlsServerChoiceProtocolIndexV1",T,I,1,{V},.parameterOwners={QC},QP},
{"xl6TlsServerContextSelectorCreateV1",T,V,2,{V,XL6_IR_C_ABI_CLOSURE_PTR},.parameterOwners={"xl6TlsParametersDestroyV1"},.resultOwner=QS,QCB,QP},
{"xl6TlsServerContextWithSelectorV1",T,V,2,{V,XL6_IR_C_ABI_CLOSURE_PTR},.parameterOwners={QS},.resultOwner=QS,QCB,QP},
{"xl6TlsServerContextHasSelectorV1",T,B,1,{V},.parameterOwners={QS},QP},
#undef QCB
#undef QP
#undef QF
#undef QS
#undef QR
#undef QC
