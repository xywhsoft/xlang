#define TD_STREAM "xl6TcpStreamDestroyV1"
#define TD_ERROR .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6TcpStreamSendNetBufferV1",T,I,3,{V,V,I},.parameterOwners={TD_STREAM,"xl6NetBufferDestroyV1"},.resultEnum=&xl6IrNetStatusEnumContract,.nonblocking=1,TD_ERROR},
{"xl6TcpStreamSendFileV1",T,I,4,{V,V,U,I},.parameterOwners={TD_STREAM,"xl6_xrt_fs_file_destroy"},.resultEnum=&xl6IrNetStatusEnumContract,TD_ERROR},
#define TD_INTO(Name,Owner) \
{"xl6TcpStreamRecvInto" #Name "V1",T,I,4,{V,V,I,I},.parameterOwners={TD_STREAM,Owner},TD_ERROR}, \
{"xl6TcpStreamRecvInto" #Name "AsyncV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,3,{V,V,I},.parameterOwners={TD_STREAM},.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=Owner,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,.resultFutureElementAbi=I,.nonblocking=1,TD_ERROR},
TD_INTO(NetBuffer,"xl6NetBufferDestroyV1")
TD_INTO(Line,"xl6NetLineFramerDestroyV1")
TD_INTO(Length,"xl6NetLengthFramerDestroyV1")
#undef TD_INTO
#undef TD_STREAM
#undef TD_ERROR
