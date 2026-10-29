#define NP "xl6NetProxyDestroyV1"
#define PH "xl6NetProxyHandshakeDestroyV1"
#define NB "xl6NetBufferDestroyV1"
#define BF "xl6BufferDestroyV1"
#define EF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6NetProxyCreateTextV1",T,V,6,{I,S,I,S,S,I},.resultOwner=NP,.parameterEnums={&xl6IrNetProxyTypeContract,NULL,NULL,NULL,NULL,&xl6IrNetProxyAuthContract},.nonblocking=1,EF},
{"xl6NetProxyCreateBytesV1",T,V,6,{I,S,I,Y,Y,I},.resultOwner=NP,.parameterEnums={&xl6IrNetProxyTypeContract,NULL,NULL,NULL,NULL,&xl6IrNetProxyAuthContract},.nonblocking=1,EF},
{"xl6NetProxyDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NP},.noRaise=1,.nativeHandlePolicy="xl6NetProxyHandlePolicyV1Get"},
{"xl6NetProxyCloneV1",T,V,1,{V},.parameterOwners={NP},.resultOwner=NP,.nonblocking=1,EF},
{"xl6NetProxyTypeV1",T,I,1,{V},.parameterOwners={NP},.resultEnum=&xl6IrNetProxyTypeContract,.pure=1,.nonblocking=1,EF},
{"xl6NetProxyAuthV1",T,I,1,{V},.parameterOwners={NP},.resultEnum=&xl6IrNetProxyAuthContract,.pure=1,.nonblocking=1,EF},
{"xl6NetProxyPortV1",T,I,1,{V},.parameterOwners={NP},.pure=1,.nonblocking=1,EF},
{"xl6NetProxyHostV1",T,S,1,{V},.parameterOwners={NP},.nonblocking=1,EF},
{"xl6NetProxyUsernameV1",T,Y,1,{V},.parameterOwners={NP},.nonblocking=1,EF},
{"xl6NetProxyPasswordV1",T,Y,1,{V},.parameterOwners={NP},.nonblocking=1,EF},
{"xl6NetProxyHandshakeCreateV1",T,V,5,{V,S,I,I,I},.parameterOwners={NP},.resultOwner=PH,.nonblocking=1,EF},
{"xl6NetProxyHandshakeDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={PH},.noRaise=1,.nativeHandlePolicy="xl6NetProxyHandshakeHandlePolicyV1Get"},
{"xl6NetProxyHandshakeStateV1",T,I,1,{V},.parameterOwners={PH},.resultEnum=&xl6IrNetProxyHandshakeStateContract,.pure=1,.nonblocking=1,EF},
{"xl6NetProxyHandshakeStepV1",T,I,1,{V},.parameterOwners={PH},.resultEnum=&xl6IrNetProxyHandshakeStateContract,.nonblocking=1,EF},
{"xl6NetProxyHandshakeSentV1",T,I,2,{V,I},.parameterOwners={PH},.nonblocking=1,EF},
{"xl6NetProxyHandshakeCodeV1",T,XL6_IR_C_ABI_PRODUCT,1,{V},.parameterOwners={PH},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLU32_V1,.pure=1,.nonblocking=1,EF},
{"xl6NetProxyHandshakeThrowV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={PH},.nonblocking=1,EF},
{"xl6NetProxyHandshakeBufferedV1",T,I,1,{V},.parameterOwners={PH},.pure=1,.nonblocking=1,EF},
{"xl6NetProxyHandshakeLimitV1",T,I,1,{V},.parameterOwners={PH},.pure=1,.noRaise=1,.nonblocking=1},
{"xl6NetProxyHandshakeAppendTextV1",T,XL6_IR_C_ABI_VOID,3,{V,S,I},.parameterOwners={PH},.nonblocking=1,EF},
{"xl6NetProxyHandshakeAppendBytesV1",T,XL6_IR_C_ABI_VOID,3,{V,Y,I},.parameterOwners={PH},.nonblocking=1,EF},
{"xl6NetProxyHandshakeAppendBufferV1",T,XL6_IR_C_ABI_VOID,3,{V,V,I},.parameterOwners={PH,BF},.nonblocking=1,EF},
{"xl6NetProxyHandshakeAppendMoveV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={PH,NB},.nonblocking=1,EF},
{"xl6NetProxyHandshakeRemainingV1",T,Y,2,{V,I},.parameterOwners={PH},.nonblocking=1,EF},
{"xl6NetProxyHandshakeMoveRemainingV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={PH,NB},.nonblocking=1,EF},
{"xl6NetProxyHandshakeOutputV1",T,Y,1,{V},.parameterOwners={PH},.nonblocking=1,EF},
{"xl6NetProxyHandshakeOutputIntoV1",T,I,3,{V,V,I},.parameterOwners={PH,BF},.nonblocking=1,EF},
{"xl6NetProxyHandshakeBoundHostV1",T,S,1,{V},.parameterOwners={PH},.nonblocking=1,EF},
{"xl6NetProxyHandshakeBoundHostBytesV1",T,Y,1,{V},.parameterOwners={PH},.nonblocking=1,EF},
{"xl6NetProxyHandshakeBoundFamilyV1",T,I,1,{V},.parameterOwners={PH},.pure=1,.nonblocking=1,EF},
{"xl6NetProxyHandshakeBoundPortV1",T,I,1,{V},.parameterOwners={PH},.pure=1,.nonblocking=1,EF},
{"xl6NetProxyHandshakeBoundAddressV1",T,V,1,{V},.parameterOwners={PH},.resultOwner="xl6NetAddressDestroyV1",.nonblocking=1,EF},
#undef EF
#undef BF
#undef NB
#undef PH
#undef NP
