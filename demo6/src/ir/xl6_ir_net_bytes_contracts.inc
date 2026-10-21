#define NY "xl6NetBytesDestroyV1"
#define NE .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6NetBytesCreateV1",T,V,0,{0},.resultOwner=NY,.nonblocking=1,NE},
{"xl6NetBytesAdoptV1",T,V,1,{XL6_IR_C_ABI_RAW_POINTER},.resultOwner=NY,.nonblocking=1,NE},
{"xl6NetBytesFromTextV1",T,V,2,{S,I},.resultOwner=NY,.nonblocking=1,NE},
{"xl6NetBytesFromBytesV1",T,V,2,{Y,I},.resultOwner=NY,.nonblocking=1,NE},
{"xl6NetBytesFromBufferV1",T,V,2,{V,I},.parameterOwners={"xl6BufferDestroyV1"},.resultOwner=NY,.nonblocking=1,NE},
{"xl6NetBytesDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NY},.noRaise=1,.nativeHandlePolicy="xl6NetBytesHandlePolicyV1Get"},
{"xl6NetBytesLengthV1",T,I,1,{V},.parameterOwners={NY},.pure=1,.noRaise=1,.nonblocking=1},
{"xl6NetBytesByteAtV1",T,I,2,{V,I},.parameterOwners={NY},.pure=1,.nonblocking=1,NE},
{"xl6NetBytesToBytesV1",T,Y,3,{V,I,I},.parameterOwners={NY},.nonblocking=1,NE},
{"xl6NetBytesAppendToV1",T,I,4,{V,V,I,I},.parameterOwners={NY,"xl6BufferDestroyV1"},.nonblocking=1,NE},
{"xl6TcpStreamSendRefV1",T,I,4,{V,V,I,I},.parameterOwners={"xl6TcpStreamDestroyV1",NY},.resultEnum=&xl6IrNetStatusEnumContract,.nonblocking=1,NE},
{"xl6TcpStreamSendMoveV1",T,I,2,{V,V},.parameterOwners={"xl6TcpStreamDestroyV1","xl6BufferDestroyV1"},.resultEnum=&xl6IrNetStatusEnumContract,.nonblocking=1,NE},
#undef NE
#undef NY
