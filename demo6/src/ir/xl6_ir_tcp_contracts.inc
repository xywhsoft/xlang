#define TC_STREAM "xl6TcpStreamDestroyV1"
#define TC_LISTENER "xl6TcpListenerDestroyV1"
#define TC_ENGINE "xl6NetEngineDestroyV1"
#define TC_ADDRESS "xl6NetAddressDestroyV1"
#define TC_STATS "xl6TcpStreamStatsDestroyV1"
#define TC_LSTATS "xl6TcpListenerStatsDestroyV1"
#define TC_ERROR .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
#define TC_SOURCE .nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1
#define TC_QUEUE .nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1
{"xl6TcpStreamCreateDefaultV1",T,V,2,{V,V},.parameterOwners={TC_ENGINE,TC_ADDRESS},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpStreamCreateV1",T,V,4,{V,V,V,I},.parameterOwners={TC_ENGINE,TC_ADDRESS,"xl6TcpStreamOptionsDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpListenerCreateV1",T,V,2,{V,V},.parameterOwners={TC_ENGINE,"xl6TcpListenOptionsDestroyV1"},.resultOwner=TC_LISTENER,TC_SOURCE,TC_ERROR},
{"xl6TcpStreamDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_STREAM},.nativeHandlePolicy="xl6TcpStreamHandlePolicyV1Get",.noRaise=1},
{"xl6TcpListenerDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_LISTENER},.nativeHandlePolicy="xl6TcpListenerHandlePolicyV1Get",.noRaise=1},
{"xl6TcpStreamCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_STREAM},.nonblocking=1,TC_ERROR},
{"xl6TcpStreamAbortV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_STREAM},.nonblocking=1,TC_ERROR},
{"xl6TcpListenerCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_LISTENER},.nonblocking=1,TC_ERROR},
{"xl6TcpStreamStateV1",T,I,1,{V},.parameterOwners={TC_STREAM},.resultEnum=&xl6IrTcpStreamStateContract,.nonblocking=1,TC_ERROR},
{"xl6TcpListenerStateV1",T,I,1,{V},.parameterOwners={TC_LISTENER},.resultEnum=&xl6IrTcpListenerStateContract,.nonblocking=1,TC_ERROR},
{"xl6TcpStreamAvailableV1",T,U,1,{V},.parameterOwners={TC_STREAM},.nonblocking=1,TC_ERROR},
{"xl6TcpStreamPendingSendV1",T,U,1,{V},.parameterOwners={TC_STREAM},.nonblocking=1,TC_ERROR},
{"xl6TcpStreamWriteLimitV1",T,U,1,{V},.parameterOwners={TC_STREAM},.nonblocking=1,TC_ERROR},
{"xl6TcpStreamWritableV1",T,U,1,{V},.parameterOwners={TC_STREAM},.nonblocking=1,TC_ERROR},
{"xl6TcpStreamSendTextV1",T,I,3,{V,S,I},.parameterOwners={TC_STREAM},.resultEnum=&xl6IrNetStatusEnumContract,.nonblocking=1,TC_ERROR},
{"xl6TcpStreamSendBytesV1",T,I,3,{V,Y,I},.parameterOwners={TC_STREAM},.resultEnum=&xl6IrNetStatusEnumContract,.nonblocking=1,TC_ERROR},
{"xl6TcpStreamSendBufferV1",T,I,3,{V,V,I},.parameterOwners={TC_STREAM,"xl6BufferDestroyV1"},.resultEnum=&xl6IrNetStatusEnumContract,.nonblocking=1,TC_ERROR},
{"xl6TcpStreamShutdownWriteV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_STREAM},.nonblocking=1,TC_ERROR},
{"xl6TcpStreamPauseReadV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_STREAM},.nonblocking=1,TC_ERROR},
{"xl6TcpStreamResumeReadV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_STREAM},.nonblocking=1,TC_ERROR},
{"xl6TcpStreamLocalV1",T,V,1,{V},.parameterOwners={TC_STREAM},.resultOwner=TC_ADDRESS,.nonblocking=1,TC_ERROR},
{"xl6TcpStreamRemoteV1",T,V,1,{V},.parameterOwners={TC_STREAM},.resultOwner=TC_ADDRESS,.nonblocking=1,TC_ERROR},
{"xl6TcpListenerLocalV1",T,V,1,{V},.parameterOwners={TC_LISTENER},.resultOwner=TC_ADDRESS,.nonblocking=1,TC_ERROR},
{"xl6TcpPrepareAcceptV1",T,V,1,{V},.parameterOwners={TC_LISTENER},.resultOwner=TC_STREAM,.nonblocking=1,TC_SOURCE,TC_ERROR},
{"xl6TcpPrepareDialV1",T,V,2,{V,V},.parameterOwners={TC_ENGINE,"xl6NetResolverDestroyV1"},.resultOwner=TC_STREAM,.nonblocking=1,TC_SOURCE,TC_ERROR},
{"xl6TcpPrepareDialAddressesV1",T,V,1,{V},.parameterOwners={TC_ENGINE},.resultOwner=TC_STREAM,.nonblocking=1,TC_SOURCE,TC_ERROR},
{"xl6TcpDialV1",T,V,5,{V,V,S,I,V},.parameterOwners={TC_ENGINE,"xl6NetResolverDestroyV1",NULL,NULL,"xl6TcpDialOptionsDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialDefaultV1",T,V,4,{V,V,S,I},.parameterOwners={TC_ENGINE,"xl6NetResolverDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialAddressesV1",T,V,3,{V,V,V},.parameterOwners={TC_ENGINE,"xl6NetAddressListDestroyV1","xl6TcpDialOptionsDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialAddressesDefaultV1",T,V,2,{V,V},.parameterOwners={TC_ENGINE,"xl6NetAddressListDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialCancelDefaultV1",T,V,5,{V,V,S,I,V},.parameterOwners={TC_ENGINE,"xl6NetResolverDestroyV1",NULL,NULL,"xl6CancellationTokenDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialCancelV1",T,V,6,{V,V,S,I,V,V},.parameterOwners={TC_ENGINE,"xl6NetResolverDestroyV1",NULL,NULL,"xl6TcpDialOptionsDestroyV1","xl6CancellationTokenDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialAddressesCancelDefaultV1",T,V,3,{V,V,V},.parameterOwners={TC_ENGINE,"xl6NetAddressListDestroyV1","xl6CancellationTokenDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialAddressesCancelV1",T,V,4,{V,V,V,V},.parameterOwners={TC_ENGINE,"xl6NetAddressListDestroyV1","xl6TcpDialOptionsDestroyV1","xl6CancellationTokenDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialUntilDefaultV1",T,V,5,{V,V,S,I,U},.parameterOwners={TC_ENGINE,"xl6NetResolverDestroyV1",NULL,NULL,NULL},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialUntilV1",T,V,6,{V,V,S,I,V,U},.parameterOwners={TC_ENGINE,"xl6NetResolverDestroyV1",NULL,NULL,"xl6TcpDialOptionsDestroyV1",NULL},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialAddressesUntilDefaultV1",T,V,3,{V,V,U},.parameterOwners={TC_ENGINE,"xl6NetAddressListDestroyV1",NULL},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialAddressesUntilV1",T,V,4,{V,V,V,U},.parameterOwners={TC_ENGINE,"xl6NetAddressListDestroyV1","xl6TcpDialOptionsDestroyV1",NULL},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialUntilCancelDefaultV1",T,V,6,{V,V,S,I,U,V},.parameterOwners={TC_ENGINE,"xl6NetResolverDestroyV1",NULL,NULL,NULL,"xl6CancellationTokenDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialUntilCancelV1",T,V,7,{V,V,S,I,V,U,V},.parameterOwners={TC_ENGINE,"xl6NetResolverDestroyV1",NULL,NULL,"xl6TcpDialOptionsDestroyV1",NULL,"xl6CancellationTokenDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialAddressesUntilCancelDefaultV1",T,V,4,{V,V,U,V},.parameterOwners={TC_ENGINE,"xl6NetAddressListDestroyV1",NULL,"xl6CancellationTokenDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpDialAddressesUntilCancelV1",T,V,5,{V,V,V,U,V},.parameterOwners={TC_ENGINE,"xl6NetAddressListDestroyV1","xl6TcpDialOptionsDestroyV1",NULL,"xl6CancellationTokenDestroyV1"},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
#define TC_DIAL_CONTEXT .nonblocking=1,TC_QUEUE,TC_ERROR, \
    .nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1
{"xl6TcpDialStartV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,5,{V,V,S,I,V},.parameterOwners={TC_STREAM,NULL,NULL,NULL,"xl6TcpDialOptionsDestroyV1"},TC_DIAL_CONTEXT},
{"xl6TcpDialStartDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,4,{V,V,S,I},.parameterOwners={TC_STREAM},TC_DIAL_CONTEXT},
{"xl6TcpDialAddressesStartV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,4,{V,V,V,V},.parameterOwners={TC_STREAM,NULL,"xl6NetAddressListDestroyV1","xl6TcpDialOptionsDestroyV1"},TC_DIAL_CONTEXT},
{"xl6TcpDialAddressesStartDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,3,{V,V,V},.parameterOwners={TC_STREAM,NULL,"xl6NetAddressListDestroyV1"},TC_DIAL_CONTEXT},
#undef TC_DIAL_CONTEXT
{"xl6TcpDialStartCancelDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,5,{V,V,S,I,V},.parameterOwners={TC_STREAM,NULL,NULL,NULL,"xl6CancellationTokenDestroyV1"},.nonblocking=1,TC_ERROR,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialStartCancelV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,6,{V,V,S,I,V,V},.parameterOwners={TC_STREAM,NULL,NULL,NULL,"xl6TcpDialOptionsDestroyV1","xl6CancellationTokenDestroyV1"},.nonblocking=1,TC_ERROR,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialStartUntilDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,5,{V,V,S,I,U},.parameterOwners={TC_STREAM,NULL,NULL,NULL,NULL},.nonblocking=1,TC_ERROR,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialStartUntilV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,6,{V,V,S,I,V,U},.parameterOwners={TC_STREAM,NULL,NULL,NULL,"xl6TcpDialOptionsDestroyV1",NULL},.nonblocking=1,TC_ERROR,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialStartUntilCancelDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,6,{V,V,S,I,U,V},.parameterOwners={TC_STREAM,NULL,NULL,NULL,NULL,"xl6CancellationTokenDestroyV1"},.nonblocking=1,TC_ERROR,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialStartUntilCancelV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,7,{V,V,S,I,V,U,V},.parameterOwners={TC_STREAM,NULL,NULL,NULL,"xl6TcpDialOptionsDestroyV1",NULL,"xl6CancellationTokenDestroyV1"},.nonblocking=1,TC_ERROR,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialAddressesStartCancelDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,4,{V,V,V,V},.parameterOwners={TC_STREAM,NULL,"xl6NetAddressListDestroyV1","xl6CancellationTokenDestroyV1"},.nonblocking=1,TC_ERROR,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialAddressesStartCancelV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,5,{V,V,V,V,V},.parameterOwners={TC_STREAM,NULL,"xl6NetAddressListDestroyV1","xl6TcpDialOptionsDestroyV1","xl6CancellationTokenDestroyV1"},.nonblocking=1,TC_ERROR,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialAddressesStartUntilDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,4,{V,V,V,U},.parameterOwners={TC_STREAM,NULL,"xl6NetAddressListDestroyV1",NULL},.nonblocking=1,TC_ERROR,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialAddressesStartUntilV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,5,{V,V,V,V,U},.parameterOwners={TC_STREAM,NULL,"xl6NetAddressListDestroyV1","xl6TcpDialOptionsDestroyV1",NULL},.nonblocking=1,TC_ERROR,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialAddressesStartUntilCancelDefaultV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,5,{V,V,V,U,V},.parameterOwners={TC_STREAM,NULL,"xl6NetAddressListDestroyV1",NULL,"xl6CancellationTokenDestroyV1"},.nonblocking=1,TC_ERROR,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
{"xl6TcpDialAddressesStartUntilCancelV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,6,{V,V,V,V,U,V},.parameterOwners={TC_STREAM,NULL,"xl6NetAddressListDestroyV1","xl6TcpDialOptionsDestroyV1",NULL,"xl6CancellationTokenDestroyV1"},.nonblocking=1,TC_ERROR,.nativeServicesRequired=(XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1),.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1},
#define TC_CLASS_FUTURE(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,V},.parameterOwners={Owner},.nonblocking=1,TC_QUEUE,TC_ERROR, \
    .nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeClassParameter=2,.nativeClassOwner=TC_STREAM,.genericFutureResult=1}
TC_CLASS_FUTURE("xl6TcpAcceptStartV1",TC_LISTENER),
TC_CLASS_FUTURE("xl6TcpConnectedStartV1",TC_STREAM),
#undef TC_CLASS_FUTURE
{"xl6TcpStreamRecvAsyncV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,I},.parameterOwners={TC_STREAM},.nonblocking=1,TC_QUEUE,TC_ERROR,.resultFutureElementAbi=Y,.resultFutureAbi=XL6_IR_ASYNC_RESULT_RETAINED_XVALUE},
{"xl6TcpStreamWaitAsyncV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,I},.parameterOwners={TC_STREAM},.parameterEnums={NULL,&xl6IrTcpWaitContract},.nonblocking=1,TC_QUEUE,TC_ERROR,.resultFutureElementAbi=B},
{"xl6TcpStreamWaitAvailableAsyncV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,I},.parameterOwners={TC_STREAM},.nonblocking=1,TC_QUEUE,TC_ERROR,.resultFutureElementAbi=B},
{"xl6TcpStreamRecvV1",T,Y,3,{V,I,I},.parameterOwners={TC_STREAM},TC_ERROR},
{"xl6TcpStreamWaitV1",T,I,3,{V,I,I},.parameterOwners={TC_STREAM},.parameterEnums={NULL,&xl6IrTcpWaitContract},.resultEnum=&xl6IrNetStatusEnumContract,TC_ERROR},
{"xl6TcpListenerAcceptV1",T,V,2,{V,I},.parameterOwners={TC_LISTENER},.resultOwner=TC_STREAM,TC_SOURCE,TC_ERROR},
{"xl6TcpStreamSnapshotV1",T,V,1,{V},.parameterOwners={TC_STREAM},.resultOwner=TC_STATS,.nonblocking=1,TC_ERROR},
{"xl6TcpListenerSnapshotV1",T,V,1,{V},.parameterOwners={TC_LISTENER},.resultOwner=TC_LSTATS,.nonblocking=1,TC_ERROR},
{"xl6TcpStreamStatsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_STATS},.nativeHandlePolicy="xl6TcpStreamStatsHandlePolicyV1Get",.noRaise=1},
{"xl6TcpListenerStatsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={TC_LSTATS},.nativeHandlePolicy="xl6TcpListenerStatsHandlePolicyV1Get",.noRaise=1},
{"xl6TcpStreamStatsStateV1",T,I,1,{V},.parameterOwners={TC_STATS},.resultEnum=&xl6IrTcpStreamStateContract,.pure=1,.noRaise=1,.nonblocking=1},
{"xl6TcpListenerStatsStateV1",T,I,1,{V},.parameterOwners={TC_LSTATS},.resultEnum=&xl6IrTcpListenerStateContract,.pure=1,.noRaise=1,.nonblocking=1},
#define XL6_TCP_STREAM_UINT(Field,Name) {"xl6TcpStreamStats" #Field "V1",T,U,1,{V},.parameterOwners={TC_STATS},.pure=1,.noRaise=1,.nonblocking=1},
#define XL6_TCP_STREAM_BOOL(Field,Name) {"xl6TcpStreamStats" #Field "V1",T,B,1,{V},.parameterOwners={TC_STATS},.pure=1,.noRaise=1,.nonblocking=1},
#define XL6_TCP_LISTENER_UINT(Field,Name) {"xl6TcpListenerStats" #Field "V1",T,U,1,{V},.parameterOwners={TC_LSTATS},.pure=1,.noRaise=1,.nonblocking=1},
#include "src/runtime/xl6_tcp_stats_fields.inc"
#undef XL6_TCP_STREAM_UINT
#undef XL6_TCP_STREAM_BOOL
#undef XL6_TCP_LISTENER_UINT
#undef TC_STREAM
#undef TC_LISTENER
#undef TC_ENGINE
#undef TC_ADDRESS
#undef TC_STATS
#undef TC_LSTATS
#undef TC_ERROR
#undef TC_SOURCE
#undef TC_QUEUE
