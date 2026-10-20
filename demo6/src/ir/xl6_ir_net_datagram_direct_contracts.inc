#define DI "xl6NetDatagramInfoDestroyV1"
#define DC "xl6NetDatagramControlDestroyV1"
#define DS "xl6NetSocketDestroyV1"
#define DA "xl6NetAddressDestroyV1"
#define DB "xl6BufferDestroyV1"
#define DF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define DP .resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64U64_V1,.resultProductEnums={&xl6IrNetStatusEnumContract}
{"xl6NetDatagramInfoCreateV1",T,V,0,{0},.resultOwner=DI,.nonblocking=1,DF},
{"xl6NetDatagramInfoDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={DI},.nativeHandlePolicy="xl6NetDatagramInfoHandlePolicyV1Get",.noRaise=1},
{"xl6NetDatagramInfoClearV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={DI},.noRaise=1,.nonblocking=1},
{"xl6NetDatagramInfoValidV1",T,B,1,{V},.parameterOwners={DI},.noRaise=1,.nonblocking=1},
{"xl6NetDatagramInfoFlagsV1",T,I,1,{V},.parameterOwners={DI},.resultEnum=&xl6IrNetDatagramMetadataEnumContract,.noRaise=1,.nonblocking=1},
{"xl6NetDatagramInfoRemoteV1",T,V,1,{V},.parameterOwners={DI},.resultOwner=DA,.nonblocking=1,DF},
{"xl6NetDatagramInfoDestinationV1",T,V,1,{V},.parameterOwners={DI},.resultOwner=DA,.nonblocking=1,DF},
#define INFO_FIELD(Name) {"xl6NetDatagramInfo" #Name "V1",T,I,1,{V},.parameterOwners={DI},.nonblocking=1,DF},
INFO_FIELD(Interface)
INFO_FIELD(HopLimit)
INFO_FIELD(TrafficClass)
INFO_FIELD(SegmentSize)
#undef INFO_FIELD
{"xl6NetDatagramControlCreateV1",T,V,0,{0},.resultOwner=DC,.nonblocking=1,DF},
{"xl6NetDatagramControlDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={DC},.nativeHandlePolicy="xl6NetDatagramControlHandlePolicyV1Get",.noRaise=1},
{"xl6NetDatagramControlFlagsV1",T,I,1,{V},.parameterOwners={DC},.resultEnum=&xl6IrNetDatagramControlFlagsEnumContract,.pure=1,.noRaise=1,.nonblocking=1},
{"xl6NetDatagramControlSourceV1",T,V,2,{V,V},.parameterOwners={DC,DA},.resultOwner=DC,.nonblocking=1,DF},
#define CONTROL_FIELD(Name) {"xl6NetDatagramControl" #Name "V1",T,V,2,{V,I},.parameterOwners={DC},.resultOwner=DC,.nonblocking=1,DF},
CONTROL_FIELD(Interface)
CONTROL_FIELD(HopLimit)
CONTROL_FIELD(TrafficClass)
CONTROL_FIELD(SegmentSize)
#undef CONTROL_FIELD
{"xl6NetDatagramControlWithoutV1",T,V,2,{V,I},.parameterOwners={DC},.resultOwner=DC,.parameterEnums={NULL,&xl6IrNetDatagramControlFlagsEnumContract},.nonblocking=1,DF},
{"xl6NetSocketMetadataAvailableV1",T,I,1,{V},.parameterOwners={DS},.resultEnum=&xl6IrNetDatagramMetadataEnumContract,DF},
{"xl6NetSocketMetadataEnabledV1",T,I,1,{V},.parameterOwners={DS},.resultEnum=&xl6IrNetDatagramMetadataEnumContract,DF},
{"xl6NetSocketControlAvailableV1",T,I,1,{V},.parameterOwners={DS},.resultEnum=&xl6IrNetDatagramControlFlagsEnumContract,DF},
{"xl6NetSocketDatagramCapabilitiesV1",T,I,1,{V},.parameterOwners={DS},.resultEnum=&xl6IrNetDatagramCapabilitiesEnumContract,DF},
{"xl6NetSocketMetadataSetV1",T,XL6_IR_C_ABI_VOID,2,{V,I},.parameterOwners={DS},.parameterEnums={NULL,&xl6IrNetDatagramMetadataEnumContract},DF},
{"xl6NetSocketMulticastJoinV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={DS,DA},DF},
{"xl6NetSocketMulticastJoinOnV1",T,XL6_IR_C_ABI_VOID,3,{V,V,V},.parameterOwners={DS,DA,DA},DF},
{"xl6NetSocketMulticastLeaveV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={DS,DA},DF},
{"xl6NetSocketMulticastLeaveOnV1",T,XL6_IR_C_ABI_VOID,3,{V,V,V},.parameterOwners={DS,DA,DA},DF},
{"xl6NetSocketMulticastLoopV1",T,XL6_IR_C_ABI_VOID,2,{V,B},.parameterOwners={DS},DF},
{"xl6NetSocketMulticastHopLimitV1",T,XL6_IR_C_ABI_VOID,2,{V,I},.parameterOwners={DS},DF},
{"xl6NetSocketMulticastInterfaceV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={DS,DA},DF},
{"xl6NetSocketMulticastInterfaceDefaultV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={DS},DF},
{"xl6NetSocketReceiveFromIntoV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,V,V,I},.parameterOwners={DS,DB,DI},DP,DF},
{"xl6NetSocketSendMessageTextV1",T,XL6_IR_C_ABI_PRODUCT,5,{V,S,V,V,I},.parameterOwners={DS,NULL,DA,DC},DP,DF},
{"xl6NetSocketSendMessageBytesV1",T,XL6_IR_C_ABI_PRODUCT,5,{V,Y,V,V,I},.parameterOwners={DS,NULL,DA,DC},DP,DF},
{"xl6NetSocketSendMessageBufferV1",T,XL6_IR_C_ABI_PRODUCT,5,{V,V,V,V,I},.parameterOwners={DS,DB,DA,DC},DP,DF},
{"xl6NetSocketSendConnectedMessageTextV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,S,V,I},.parameterOwners={DS,NULL,DC},DP,DF},
{"xl6NetSocketSendConnectedMessageBytesV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,Y,V,I},.parameterOwners={DS,NULL,DC},DP,DF},
{"xl6NetSocketSendConnectedMessageBufferV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,V,V,I},.parameterOwners={DS,DB,DC},DP,DF},
#undef DI
#undef DC
#undef DS
#undef DA
#undef DB
#undef DF
#undef DP
