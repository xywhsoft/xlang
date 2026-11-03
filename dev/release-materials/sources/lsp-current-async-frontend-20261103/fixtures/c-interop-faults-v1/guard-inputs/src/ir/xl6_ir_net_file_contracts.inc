#define NF "xl6NetFileDestroyV1"
#define NF_ERROR .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define NF_FUTURE .nonblocking=1,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,NF_ERROR
{"xl6NetFileCreateV1",T,V,6,{V,S,I,I,I,I},.parameterOwners={"xl6NetEngineDestroyV1"},.resultOwner=NF,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,NF_ERROR},
{"xl6NetFileDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={NF},.nativeHandlePolicy="xl6NetFileHandlePolicyV1Get",.noRaise=1},
{"xl6NetFileReadV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,3,{V,U,I},.parameterOwners={NF},.resultFutureElementAbi=Y,.resultFutureAbi=XL6_IR_ASYNC_RESULT_RETAINED_XVALUE,NF_FUTURE},
{"xl6NetFileReadCancelV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,4,{V,U,I,V},.parameterOwners={NF,NULL,NULL,"xl6CancellationTokenDestroyV1"},.resultFutureElementAbi=Y,.resultFutureAbi=XL6_IR_ASYNC_RESULT_RETAINED_XVALUE,NF_FUTURE},
{"xl6NetFileCloseV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,1,{V},.parameterOwners={NF},.resultFutureAbi=XL6_IR_ASYNC_RESULT_VOID,NF_FUTURE},
#define NF_WRITE(Name,Abi,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,4,{V,U,Abi,I},.parameterOwners={NF,NULL,Owner},.resultFutureElementAbi=I,NF_FUTURE}
#define NF_WRITE_CANCEL(Name,Abi,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,5,{V,U,Abi,V,I},.parameterOwners={NF,NULL,Owner,"xl6CancellationTokenDestroyV1"},.resultFutureElementAbi=I,NF_FUTURE}
NF_WRITE("xl6NetFileWriteTextV1",S,NULL),
NF_WRITE_CANCEL("xl6NetFileWriteTextCancelV1",S,NULL),
NF_WRITE("xl6NetFileWriteBytesV1",Y,NULL),
NF_WRITE_CANCEL("xl6NetFileWriteBytesCancelV1",Y,NULL),
NF_WRITE("xl6NetFileWriteBufferV1",V,"xl6BufferDestroyV1"),
NF_WRITE_CANCEL("xl6NetFileWriteBufferCancelV1",V,"xl6BufferDestroyV1"),
#undef NF_WRITE_CANCEL
#undef NF_WRITE
#undef NF_FUTURE
#undef NF_ERROR
#undef NF
