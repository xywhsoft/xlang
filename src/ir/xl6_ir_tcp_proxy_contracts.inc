/* Complete typed/effect/ownership contracts. No Cgen name inference. */
#define PC_OPTIONS "xl6TcpProxyDialOptionsDestroyV1"
#define PC_TRANSPORT "xl6TcpDialOptionsDestroyV1"
#define PC_RESULT "xl6TcpStreamDestroyV1"
#define PC_ENGINE "xl6NetEngineDestroyV1"
#define PC_RESOLVER "xl6NetResolverDestroyV1"
#define PC_PROXY "xl6NetProxyDestroyV1"
#define PC_CANCEL "xl6CancellationTokenDestroyV1"
#define PC_ERROR .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6TcpProxyDialOptionsCreateV1",T,V,0,{0},.resultOwner=PC_OPTIONS,.nonblocking=1,PC_ERROR},
{"xl6TcpProxyDialOptionsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={PC_OPTIONS},.nativeHandlePolicy="xl6TcpProxyDialOptionsHandlePolicyV1Get",.noRaise=1},
{"xl6TcpProxyDialOptionsTransportV1",T,V,1,{V},.parameterOwners={PC_OPTIONS},.resultOwner=PC_TRANSPORT,.nonblocking=1,PC_ERROR},
{"xl6TcpProxyDialOptionsWithTransportV1",T,V,2,{V,V},.parameterOwners={PC_OPTIONS,PC_TRANSPORT},.resultOwner=PC_OPTIONS,.nonblocking=1,PC_ERROR},
{"xl6TcpProxyDialOptionsTimeoutV1",T,I,1,{V},.parameterOwners={PC_OPTIONS},.pure=1,.noRaise=1,.nonblocking=1},
{"xl6TcpProxyDialOptionsWithTimeoutV1",T,V,2,{V,I},.parameterOwners={PC_OPTIONS},.resultOwner=PC_OPTIONS,.nonblocking=1,PC_ERROR},
{"xl6TcpProxyDialOptionsReceiveLimitV1",T,U,1,{V},.parameterOwners={PC_OPTIONS},.pure=1,.noRaise=1,.nonblocking=1},
{"xl6TcpProxyDialOptionsWithReceiveLimitV1",T,V,2,{V,U},.parameterOwners={PC_OPTIONS},.resultOwner=PC_OPTIONS,.nonblocking=1,PC_ERROR},
{"xl6TcpProxyDialDefaultV1",T,V,5,{V,V,V,S,I},.parameterOwners={PC_ENGINE,PC_RESOLVER,PC_PROXY,NULL,NULL},.resultOwner=PC_RESULT,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,PC_ERROR},
{"xl6TcpProxyDialStartDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,5,{V,V,V,S,I},.parameterOwners={PC_RESULT,NULL,PC_PROXY,NULL,NULL},.nonblocking=1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=PC_RESULT,.genericFutureResult=1,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,PC_ERROR},
{"xl6TcpProxyDialV1",T,V,6,{V,V,V,S,I,V},.parameterOwners={PC_ENGINE,PC_RESOLVER,PC_PROXY,NULL,NULL,PC_OPTIONS},.resultOwner=PC_RESULT,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,PC_ERROR},
{"xl6TcpProxyDialStartV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,6,{V,V,V,S,I,V},.parameterOwners={PC_RESULT,NULL,PC_PROXY,NULL,NULL,PC_OPTIONS},.nonblocking=1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=PC_RESULT,.genericFutureResult=1,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,PC_ERROR},
{"xl6TcpProxyDialCancelDefaultV1",T,V,6,{V,V,V,S,I,V},.parameterOwners={PC_ENGINE,PC_RESOLVER,PC_PROXY,NULL,NULL,PC_CANCEL},.resultOwner=PC_RESULT,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,PC_ERROR},
{"xl6TcpProxyDialStartCancelDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,6,{V,V,V,S,I,V},.parameterOwners={PC_RESULT,NULL,PC_PROXY,NULL,NULL,PC_CANCEL},.nonblocking=1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=PC_RESULT,.genericFutureResult=1,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,PC_ERROR},
{"xl6TcpProxyDialCancelV1",T,V,7,{V,V,V,S,I,V,V},.parameterOwners={PC_ENGINE,PC_RESOLVER,PC_PROXY,NULL,NULL,PC_OPTIONS,PC_CANCEL},.resultOwner=PC_RESULT,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,PC_ERROR},
{"xl6TcpProxyDialStartCancelV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,7,{V,V,V,S,I,V,V},.parameterOwners={PC_RESULT,NULL,PC_PROXY,NULL,NULL,PC_OPTIONS,PC_CANCEL},.nonblocking=1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=PC_RESULT,.genericFutureResult=1,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,PC_ERROR},
{"xl6TcpProxyDialUntilDefaultV1",T,V,6,{V,V,V,S,I,U},.parameterOwners={PC_ENGINE,PC_RESOLVER,PC_PROXY,NULL,NULL,NULL},.resultOwner=PC_RESULT,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,PC_ERROR},
{"xl6TcpProxyDialStartUntilDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,6,{V,V,V,S,I,U},.parameterOwners={PC_RESULT,NULL,PC_PROXY,NULL,NULL,NULL},.nonblocking=1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=PC_RESULT,.genericFutureResult=1,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),PC_ERROR},
{"xl6TcpProxyDialUntilV1",T,V,7,{V,V,V,S,I,V,U},.parameterOwners={PC_ENGINE,PC_RESOLVER,PC_PROXY,NULL,NULL,PC_OPTIONS,NULL},.resultOwner=PC_RESULT,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,PC_ERROR},
{"xl6TcpProxyDialStartUntilV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,7,{V,V,V,S,I,V,U},.parameterOwners={PC_RESULT,NULL,PC_PROXY,NULL,NULL,PC_OPTIONS,NULL},.nonblocking=1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=PC_RESULT,.genericFutureResult=1,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),PC_ERROR},
{"xl6TcpProxyDialUntilCancelDefaultV1",T,V,7,{V,V,V,S,I,U,V},.parameterOwners={PC_ENGINE,PC_RESOLVER,PC_PROXY,NULL,NULL,NULL,PC_CANCEL},.resultOwner=PC_RESULT,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,PC_ERROR},
{"xl6TcpProxyDialStartUntilCancelDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,7,{V,V,V,S,I,U,V},.parameterOwners={PC_RESULT,NULL,PC_PROXY,NULL,NULL,NULL,PC_CANCEL},.nonblocking=1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=PC_RESULT,.genericFutureResult=1,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),PC_ERROR},
{"xl6TcpProxyDialUntilCancelV1",T,V,8,{V,V,V,S,I,V,U,V},.parameterOwners={PC_ENGINE,PC_RESOLVER,PC_PROXY,NULL,NULL,PC_OPTIONS,NULL,PC_CANCEL},.resultOwner=PC_RESULT,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,PC_ERROR},
{"xl6TcpProxyDialStartUntilCancelV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,8,{V,V,V,S,I,V,U,V},.parameterOwners={PC_RESULT,NULL,PC_PROXY,NULL,NULL,PC_OPTIONS,NULL,PC_CANCEL},.nonblocking=1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=PC_RESULT,.genericFutureResult=1,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),PC_ERROR},
#undef PC_OPTIONS
#undef PC_TRANSPORT
#undef PC_RESULT
#undef PC_ENGINE
#undef PC_RESOLVER
#undef PC_PROXY
#undef PC_CANCEL
#undef PC_ERROR
