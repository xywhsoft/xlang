#define SO "xl6NetSocketDestroyV1"
#define SA "xl6NetAddressDestroyV1"
#define SB "xl6BufferDestroyV1"
#define SF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define SP .resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64U64_V1,.resultProductEnums={&xl6IrNetStatusEnumContract}
{"xl6NetSocketEmptyV1",T,V,0,{0},.resultOwner=SO,.nonblocking=1,SF},
{"xl6NetSocketCreateV1",T,V,3,{I,I,B},.resultOwner=SO,.parameterEnums={&xl6IrNetSocketKindEnumContract},SF},
{"xl6NetSocketDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={SO},.nativeHandlePolicy="xl6NetSocketHandlePolicyV1Get",SF},
{"xl6NetSocketCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={SO},SF},
{"xl6NetSocketClosedV1",T,B,1,{V},.parameterOwners={SO},.noRaise=1,.nonblocking=1},
{"xl6NetSocketFamilyV1",T,I,1,{V},.parameterOwners={SO},.nonblocking=1,SF},
{"xl6NetSocketKindV1",T,I,1,{V},.parameterOwners={SO},.resultEnum=&xl6IrNetSocketKindEnumContract,.nonblocking=1,SF},
{"xl6NetSocketAvailableV1",T,U,1,{V},.parameterOwners={SO},SF},
{"xl6NetSocketSetV1",T,XL6_IR_C_ABI_VOID,3,{V,I,I},.parameterOwners={SO},.parameterEnums={NULL,&xl6IrNetSocketOptionEnumContract},SF},
{"xl6NetSocketGetV1",T,I,2,{V,I},.parameterOwners={SO},.parameterEnums={NULL,&xl6IrNetSocketOptionEnumContract},SF},
{"xl6NetSocketBindV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={SO,SA},SF},
{"xl6NetSocketListenV1",T,XL6_IR_C_ABI_VOID,2,{V,I},.parameterOwners={SO},SF},
{"xl6NetSocketConnectV1",T,I,2,{V,V},.parameterOwners={SO,SA},.resultEnum=&xl6IrNetStatusEnumContract,SF},
{"xl6NetSocketFinishConnectV1",T,I,1,{V},.parameterOwners={SO},.resultEnum=&xl6IrNetStatusEnumContract,SF},
{"xl6NetSocketAcceptIntoV1",T,I,2,{V,V},.parameterOwners={SO,SO},.resultEnum=&xl6IrNetStatusEnumContract,SF},
{"xl6NetSocketShutdownV1",T,XL6_IR_C_ABI_VOID,2,{V,I},.parameterOwners={SO},.parameterEnums={NULL,&xl6IrNetShutdownEnumContract},SF},
{"xl6NetSocketLocalV1",T,V,1,{V},.parameterOwners={SO},.resultOwner=SA,SF},
{"xl6NetSocketRemoteV1",T,V,1,{V},.parameterOwners={SO},.resultOwner=SA,SF},
{"xl6NetSocketSendBytesV1",T,XL6_IR_C_ABI_PRODUCT,3,{V,Y,I},.parameterOwners={SO},SP,SF},
{"xl6NetSocketSendTextV1",T,XL6_IR_C_ABI_PRODUCT,3,{V,S,I},.parameterOwners={SO},SP,SF},
{"xl6NetSocketSendBufferV1",T,XL6_IR_C_ABI_PRODUCT,3,{V,V,I},.parameterOwners={SO,SB},SP,SF},
{"xl6NetSocketSendToBytesV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,Y,V,I},.parameterOwners={SO,NULL,SA},SP,SF},
{"xl6NetSocketSendToTextV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,S,V,I},.parameterOwners={SO,NULL,SA},SP,SF},
{"xl6NetSocketSendToBufferV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,V,V,I},.parameterOwners={SO,SB,SA},SP,SF},
{"xl6NetSocketReceiveIntoV1",T,XL6_IR_C_ABI_PRODUCT,3,{V,V,I},.parameterOwners={SO,SB},SP,SF},
#undef SO
#undef SA
#undef SB
#undef SF
#undef SP
