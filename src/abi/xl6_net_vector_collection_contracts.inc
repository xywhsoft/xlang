#define SO "xl6NetSocketDestroyV1"
#define SA "xl6NetAddressDestroyV1"
#define SB "xl6BufferDestroyV1"
#define SI "xl6NetDatagramInfoDestroyV1"
#define SC "xl6NetDatagramControlDestroyV1"
#define SF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define SP .resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64U64_V1,.resultProductEnums={&xl6IrNetStatusEnumContract}
#define XL6_NET_VECTOR_PRIMITIVE_CONTRACT(Name,Element) \
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketSendVector" #Name "V1",T,XL6_IR_C_ABI_PRODUCT,3,{V,V,I},.parameterOwners={SO},.parameterArrayItemAbis={0,Element},SP,SF) \
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketSendVectorTo" #Name "V1",T,XL6_IR_C_ABI_PRODUCT,4,{V,V,V,I},.parameterOwners={SO,NULL,SA},.parameterArrayItemAbis={0,Element},SP,SF) \
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketSendMessageVector" #Name "V1",T,XL6_IR_C_ABI_PRODUCT,5,{V,V,V,V,I},.parameterOwners={SO,NULL,SA,SC},.parameterArrayItemAbis={0,Element},SP,SF) \
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketSendConnectedMessageVector" #Name "V1",T,XL6_IR_C_ABI_PRODUCT,4,{V,V,V,I},.parameterOwners={SO,NULL,SC},.parameterArrayItemAbis={0,Element},SP,SF)
XL6_NET_VECTOR_PRIMITIVE_CONTRACT(Text,S)
XL6_NET_VECTOR_PRIMITIVE_CONTRACT(Bytes,Y)
#undef XL6_NET_VECTOR_PRIMITIVE_CONTRACT
#define SX .nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1,.nativeClassArrayParameter=2,.nativeClassArrayOwner=SB
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketSendVectorBufferV1",T,XL6_IR_C_ABI_PRODUCT,3,{V,V,I},.parameterOwners={SO},SX,SP,SF)
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketSendVectorToBufferV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,V,V,I},.parameterOwners={SO,NULL,SA},SX,SP,SF)
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketSendMessageVectorBufferV1",T,XL6_IR_C_ABI_PRODUCT,5,{V,V,V,V,I},.parameterOwners={SO,NULL,SA,SC},SX,SP,SF)
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketSendConnectedMessageVectorBufferV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,V,V,I},.parameterOwners={SO,NULL,SC},SX,SP,SF)
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketReceiveVectorIntoV1",T,XL6_IR_C_ABI_PRODUCT,3,{V,V,I},.parameterOwners={SO},SX,SP,SF)
XL6_NATIVE_COLLECTION_BRIDGE("xl6NetSocketReceiveFromVectorIntoV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,V,V,I},.parameterOwners={SO,NULL,SI},SX,SP,SF)
#undef SX
#undef SP
#undef SF
#undef SO
#undef SA
#undef SB
#undef SI
#undef SC
