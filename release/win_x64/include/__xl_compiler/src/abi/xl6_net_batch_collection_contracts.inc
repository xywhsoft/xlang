#define NP "xl6NetDatagramPacketDestroyV1"
#define NB "xl6BufferDestroyV1"
#define NS "xl6NetSocketDestroyV1"
#define NG "xl6NetBatchProgressDestroyV1"
#define NF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetDatagramPacketCreateBufferV1",T,V,1,{V},.resultOwner=NP,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=1,.nativeClassOwner=NB,.nonblocking=1,NF)
#define NX .nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1,.nativeClassArrayParameter=2,.nativeClassArrayOwner=NP
#define NR .resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64U64_V1,.resultProductEnums={&xl6IrNetStatusEnumContract}
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketSendBatchV1",T,XL6_IR_C_ABI_PRODUCT,5,{V,V,V,I,I},.parameterOwners={NS,NULL,NG},NX,NR,NF)
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketReceiveBatchV1",T,XL6_IR_C_ABI_PRODUCT,6,{V,V,V,I,I,I},.parameterOwners={NS,NULL,NG},NX,NR,NF)
#undef NR
#undef NX
#undef NF
#undef NG
#undef NS
#undef NB
#undef NP
