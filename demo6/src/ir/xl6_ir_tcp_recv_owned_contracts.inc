#define RO_STREAM "xl6TcpStreamDestroyV1"
#define RO_TOKEN "xl6CancellationTokenDestroyV1"
#define RO_ERROR .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define RO_FUTURE .nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner="xl6NetBytesDestroyV1",.genericFutureResult=1,.nonblocking=1
{"xl6TcpStreamRecvOwnedV1",T,V,3,{V,I,I},.parameterOwners={RO_STREAM},.resultOwner="xl6NetBytesDestroyV1",RO_ERROR},
{"xl6TcpStreamRecvOwnedCancelV1",T,V,4,{V,V,I,I},.parameterOwners={RO_STREAM,RO_TOKEN},.resultOwner="xl6NetBytesDestroyV1",RO_ERROR},
{"xl6TcpStreamRecvOwnedUntilV1",T,V,3,{V,U,I},.parameterOwners={RO_STREAM},.resultOwner="xl6NetBytesDestroyV1",RO_ERROR},
{"xl6TcpStreamRecvOwnedUntilCancelV1",T,V,4,{V,U,V,I},.parameterOwners={RO_STREAM,NULL,RO_TOKEN},.resultOwner="xl6NetBytesDestroyV1",RO_ERROR},
{"xl6TcpStreamRecvOwnedStartV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,3,{V,V,I},.parameterOwners={RO_STREAM},.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,RO_FUTURE,RO_ERROR},
{"xl6TcpStreamRecvOwnedStartCancelV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,4,{V,V,V,I},.parameterOwners={RO_STREAM,NULL,RO_TOKEN},.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,RO_FUTURE,RO_ERROR},
{"xl6TcpStreamRecvOwnedStartUntilV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,4,{V,V,U,I},.parameterOwners={RO_STREAM},.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),RO_FUTURE,RO_ERROR},
{"xl6TcpStreamRecvOwnedStartUntilCancelV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,5,{V,V,U,V,I},.parameterOwners={RO_STREAM,NULL,NULL,RO_TOKEN},.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),RO_FUTURE,RO_ERROR},
#undef RO_FUTURE
#undef RO_ERROR
#undef RO_STREAM
#undef RO_TOKEN
