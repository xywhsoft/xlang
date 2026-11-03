#define DE "xl6NetDatagramErrorInfoDestroyV1"
#define DS "xl6NetSocketDestroyV1"
#define DA "xl6NetAddressDestroyV1"
#define DB "xl6BufferDestroyV1"
#define DF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6NetDatagramErrorInfoCreateV1",T,V,0,{0},.resultOwner=DE,.nonblocking=1,DF},
{"xl6NetDatagramErrorInfoDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={DE},.nativeHandlePolicy="xl6NetDatagramErrorInfoHandlePolicyV1Get",.noRaise=1},
{"xl6NetDatagramErrorInfoClearV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={DE},.noRaise=1,.nonblocking=1},
{"xl6NetDatagramErrorInfoValidV1",T,B,1,{V},.parameterOwners={DE},.noRaise=1,.nonblocking=1},
{"xl6NetDatagramErrorInfoFlagsV1",T,I,1,{V},.parameterOwners={DE},.resultEnum=&xl6IrNetDatagramErrorFlagsEnumContract,.noRaise=1,.nonblocking=1},
{"xl6NetDatagramErrorInfoOriginV1",T,I,1,{V},.parameterOwners={DE},.resultEnum=&xl6IrNetDatagramErrorOriginEnumContract,.nonblocking=1,DF},
#define ERROR_FIELD(Name) {"xl6NetDatagramErrorInfo" #Name "V1",T,I,1,{V},.parameterOwners={DE},.nonblocking=1,DF},
ERROR_FIELD(KindCode)
ERROR_FIELD(SystemCode)
ERROR_FIELD(Type)
ERROR_FIELD(Code)
ERROR_FIELD(Info)
ERROR_FIELD(Data)
#undef ERROR_FIELD
{"xl6NetDatagramErrorInfoPathMtuV1",T,U,1,{V},.parameterOwners={DE},.nonblocking=1,DF},
{"xl6NetDatagramErrorInfoRemoteV1",T,V,1,{V},.parameterOwners={DE},.resultOwner=DA,.nonblocking=1,DF},
{"xl6NetDatagramErrorInfoOffenderV1",T,V,1,{V},.parameterOwners={DE},.resultOwner=DA,.nonblocking=1,DF},
{"xl6NetSocketReceiveErrorIntoV1",T,XL6_IR_C_ABI_PRODUCT,4,{V,V,V,I},.parameterOwners={DS,DB,DE},
    .resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64U64_V1,.resultProductEnums={&xl6IrNetStatusEnumContract},DF},
{"xl6NetSocketPathMtuModeSetV1",T,XL6_IR_C_ABI_VOID,2,{V,I},.parameterOwners={DS},.parameterEnums={NULL,&xl6IrNetPathMtuModeEnumContract},DF},
{"xl6NetSocketPathMtuModeV1",T,I,1,{V},.parameterOwners={DS},.resultEnum=&xl6IrNetPathMtuModeEnumContract,DF},
{"xl6NetSocketPathMtuV1",T,U,1,{V},.parameterOwners={DS},DF},
{"xl6NetSocketDatagramErrorsSetV1",T,XL6_IR_C_ABI_VOID,2,{V,B},.parameterOwners={DS},DF},
{"xl6NetSocketDatagramErrorsV1",T,B,1,{V},.parameterOwners={DS},DF},
#undef DE
#undef DS
#undef DA
#undef DB
#undef DF
