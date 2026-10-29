#define NP "xl6NetDatagramPacketDestroyV1"
#define NG "xl6NetBatchProgressDestroyV1"
#define NF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6NetDatagramPacketCreateV1",T,V,1,{I},.resultOwner=NP,.nonblocking=1,NF},
{"xl6NetDatagramPacketCreateTextV1",T,V,2,{S,I},.resultOwner=NP,.nonblocking=1,NF},
{"xl6NetDatagramPacketCreateBytesV1",T,V,2,{Y,I},.resultOwner=NP,.nonblocking=1,NF},
{"xl6NetDatagramPacketDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NP},.nativeHandlePolicy="xl6NetDatagramPacketHandlePolicyV1Get",NF},
{"xl6NetDatagramPacketBufferHandleV1",T,V,1,{V},.parameterOwners={NP},.resultOwner="xl6BufferDestroyV1",.managedHandleResult=1,.nonblocking=1,NF},
{"xl6NetDatagramPacketTargetSetV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={NP,"xl6NetAddressDestroyV1"},.nonblocking=1,NF},
{"xl6NetDatagramPacketTargetClearV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NP},.nonblocking=1,.noRaise=1},
{"xl6NetDatagramPacketHasTargetV1",T,B,1,{V},.parameterOwners={NP},.nonblocking=1,.noRaise=1},
{"xl6NetDatagramPacketTargetV1",T,V,1,{V},.parameterOwners={NP},.resultOwner="xl6NetAddressDestroyV1",.nonblocking=1,NF},
{"xl6NetDatagramPacketValidV1",T,B,1,{V},.parameterOwners={NP},.nonblocking=1,.noRaise=1},
{"xl6NetDatagramPacketStatusV1",T,I,1,{V},.parameterOwners={NP},.resultEnum=&xl6IrNetStatusEnumContract,.nonblocking=1,NF},
{"xl6NetDatagramPacketInfoV1",T,V,1,{V},.parameterOwners={NP},.resultOwner="xl6NetDatagramInfoDestroyV1",.nonblocking=1,NF},
{"xl6NetDatagramPacketClearV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NP},.nonblocking=1,NF},
{"xl6NetBatchProgressCreateV1",T,V,0,{0},.resultOwner=NG,.nonblocking=1,NF},
{"xl6NetBatchProgressDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NG},.nativeHandlePolicy="xl6NetBatchProgressHandlePolicyV1Get",.noRaise=1},
{"xl6NetBatchProgressClearV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NG},.nonblocking=1,.noRaise=1},
{"xl6NetBatchProgressAttemptedV1",T,B,1,{V},.parameterOwners={NG},.nonblocking=1,.noRaise=1},
{"xl6NetBatchProgressCountV1",T,U,1,{V},.parameterOwners={NG},.nonblocking=1,.noRaise=1},
{"xl6NetBatchProgressStartV1",T,U,1,{V},.parameterOwners={NG},.nonblocking=1,.noRaise=1},
{"xl6NetBatchProgressStatusV1",T,I,1,{V},.parameterOwners={NG},.resultEnum=&xl6IrNetStatusEnumContract,.nonblocking=1,.noRaise=1},
#undef NP
#undef NG
#undef NF
