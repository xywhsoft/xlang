#define EC "xl6NetEngineConfigDestroyV1"
#define EE "xl6NetEngineDestroyV1"
#define ES "xl6NetEngineSnapshotDestroyV1"
#define EF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
{"xl6NetEngineConfigCreateV1",T,V,0,{0},.resultOwner=EC,.nonblocking=1,EF},
{"xl6NetEngineConfigDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={EC},.nativeHandlePolicy="xl6NetEngineConfigHandlePolicyV1Get",.noRaise=1},
{"xl6NetEngineConfigBackendV1",T,I,1,{V},.parameterOwners={EC},.resultEnum=&xl6IrNetBackendEnumContract,.pure=1,.noRaise=1,.nonblocking=1},
{"xl6NetEngineConfigWithBackendV1",T,V,2,{V,I},.parameterOwners={EC},.resultOwner=EC,.parameterEnums={NULL,&xl6IrNetBackendEnumContract},.nonblocking=1,EF},
{"xl6NetEngineConfigWorkersV1",T,I,1,{V},.parameterOwners={EC},.pure=1,.noRaise=1,.nonblocking=1},
{"xl6NetEngineConfigWithWorkersV1",T,V,2,{V,I},.parameterOwners={EC},.resultOwner=EC,.nonblocking=1,EF},
{"xl6NetEngineConfigIdleWaitV1",T,I,1,{V},.parameterOwners={EC},.pure=1,.noRaise=1,.nonblocking=1},
{"xl6NetEngineConfigWithIdleWaitV1",T,V,2,{V,I},.parameterOwners={EC},.resultOwner=EC,.nonblocking=1,EF},
{"xl6NetEngineConfigBufferBudgetV1",T,U,1,{V},.parameterOwners={EC},.pure=1,.noRaise=1,.nonblocking=1},
{"xl6NetEngineConfigWithBufferBudgetV1",T,V,2,{V,U},.parameterOwners={EC},.resultOwner=EC,.nonblocking=1,EF},
{"xl6NetEngineConfigBufferBlockSizeV1",T,U,2,{V,I},.parameterOwners={EC},.pure=1,.nonblocking=1,EF},
{"xl6NetEngineConfigBufferCacheLimitV1",T,U,2,{V,I},.parameterOwners={EC},.pure=1,.nonblocking=1,EF},
{"xl6NetEngineConfigWithBufferClassV1",T,V,4,{V,I,U,U},.parameterOwners={EC},.resultOwner=EC,.nonblocking=1,EF},
#define XL6_NET_ENGINE_CONFIG_SIZE(Field,Name) \
{"xl6NetEngineConfig" #Field "V1",T,U,1,{V},.parameterOwners={EC},.pure=1,.noRaise=1,.nonblocking=1}, \
{"xl6NetEngineConfigWith" #Field "V1",T,V,2,{V,U},.parameterOwners={EC},.resultOwner=EC,.nonblocking=1,EF},
#include "src/runtime/xl6_net_engine_config_fields.inc"
#undef XL6_NET_ENGINE_CONFIG_SIZE
{"xl6NetEngineCreateDefaultV1",T,V,0,{0},.resultOwner=EE,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,EF},
{"xl6NetEngineCreateWorkersV1",T,V,1,{I},.resultOwner=EE,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,EF},
{"xl6NetEngineCreateV1",T,V,1,{V},.resultOwner=EE,.parameterOwners={EC},.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,EF},
{"xl6NetEngineDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={EE},.nativeHandlePolicy="xl6NetEngineHandlePolicyV1Get",.noRaise=1},
{"xl6NetEngineStartV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={EE},EF},
{"xl6NetEngineStopV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={EE},EF},
{"xl6NetEngineTryStopV1",T,B,1,{V},.parameterOwners={EE},EF},
{"xl6NetEngineCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={EE},.nonblocking=1,EF},
{"xl6NetEngineWaitV1",T,B,2,{V,I},.parameterOwners={EE},EF},
{"xl6NetEnginePostV1",T,XL6_IR_C_ABI_VOID,3,{V,XL6_IR_C_ABI_CLOSURE_PTR,U},.parameterOwners={EE},.parameterNullaryCallableResultAbis={0,XL6_IR_C_ABI_VOID},EF},
{"xl6NetEngineAfterV1",T,U,4,{V,I,XL6_IR_C_ABI_CLOSURE_PTR,U},.parameterOwners={EE},.parameterNullaryCallableResultAbis={0,0,XL6_IR_C_ABI_VOID},EF},
{"xl6NetEngineCancelTimerV1",T,XL6_IR_C_ABI_VOID,2,{V,U},.parameterOwners={EE},EF},
{"xl6NetEngineThrowV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={EE},.nonblocking=1,EF},
{"xl6NetEngineFailureCountV1",T,U,1,{V},.parameterOwners={EE},.nonblocking=1,EF},
{"xl6NetEngineFailureSaturatedV1",T,B,1,{V},.parameterOwners={EE},.nonblocking=1,EF},
{"xl6NetEngineConfigurationV1",T,V,1,{V},.parameterOwners={EE},.resultOwner=EC,.nonblocking=1,EF},
{"xl6NetEngineSnapshotV1",T,V,1,{V},.parameterOwners={EE},.resultOwner=ES,.nonblocking=1,EF},
{"xl6NetEngineStateV1",T,I,1,{V},.parameterOwners={EE},.resultEnum=&xl6IrNetEngineStateEnumContract,.nonblocking=1,EF},
{"xl6NetEngineWorkersV1",T,U,1,{V},.parameterOwners={EE},.nonblocking=1,EF},
{"xl6NetEngineBackendV1",T,I,2,{V,I},.parameterOwners={EE},.resultEnum=&xl6IrNetBackendEnumContract,.nonblocking=1,EF},
{"xl6NetEngineCapabilitiesV1",T,I,2,{V,I},.parameterOwners={EE},.nonblocking=1,EF},
{"xl6NetEngineSnapshotDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={ES},.nativeHandlePolicy="xl6NetEngineSnapshotHandlePolicyV1Get",.noRaise=1},
{"xl6NetEngineSnapshotStateV1",T,I,1,{V},.parameterOwners={ES},.resultEnum=&xl6IrNetEngineStateEnumContract,.pure=1,.noRaise=1,.nonblocking=1},
#define XL6_NET_ENGINE_STAT(Field,Name) {"xl6NetEngineSnapshot" #Field "V1",T,U,1,{V},.parameterOwners={ES},.pure=1,.noRaise=1,.nonblocking=1},
#include "src/runtime/xl6_net_engine_stats_fields.inc"
#undef XL6_NET_ENGINE_STAT
#undef EF
#undef EC
#undef EE
#undef ES
