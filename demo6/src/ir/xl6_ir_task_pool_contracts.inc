{"xl6TaskPoolCreateLanguageV1",T,V,3,{I,I,I},.resultOwner="xl6TaskPoolDestroyLanguageV1",.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6TaskPoolDestroyLanguageV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6TaskPoolDestroyLanguageV1"},.nativeHandlePolicy="xl6TaskPoolHandlePolicyV1Get",.noRaise=1},
#define POOL_CONTROL(Name) {"xl6TaskPool" Name "LanguageV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6TaskPoolDestroyLanguageV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE}
POOL_CONTROL("Start"),POOL_CONTROL("Close"),POOL_CONTROL("Cancel"),
#undef POOL_CONTROL
{"xl6TaskPoolWaitLanguageV1",T,B,2,{V,I},.parameterOwners={"xl6TaskPoolDestroyLanguageV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6TaskPoolWaitUntilLanguageV1",T,B,2,{V,U},.parameterOwners={"xl6TaskPoolDestroyLanguageV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6TaskPoolWaitCancelLanguageV1",T,B,3,{V,V,I},.parameterOwners={"xl6TaskPoolDestroyLanguageV1","xl6CancellationTokenDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6TaskPoolWaitUntilCancelLanguageV1",T,B,3,{V,U,V},.parameterOwners={"xl6TaskPoolDestroyLanguageV1",NULL,"xl6CancellationTokenDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6TaskPoolSnapshotLanguageV1",T,V,1,{V},.parameterOwners={"xl6TaskPoolDestroyLanguageV1"},.resultOwner="xl6TaskPoolSnapshotDestroyLanguageV1",.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
{"xl6TaskPoolSnapshotDestroyLanguageV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6TaskPoolSnapshotDestroyLanguageV1"},.nativeHandlePolicy="xl6TaskPoolSnapshotHandlePolicyV1Get",.noRaise=1},
#define POOL_STAT(Name,Result) {"xl6TaskPoolSnapshot" Name "LanguageV1",T,Result,1,{V},.parameterOwners={"xl6TaskPoolSnapshotDestroyLanguageV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE}
POOL_STAT("Threads",U),POOL_STAT("QueueLimit",U),POOL_STAT("Queued",U),POOL_STAT("Running",U),
POOL_STAT("Submitted",U),POOL_STAT("Completed",U),POOL_STAT("Succeeded",U),POOL_STAT("Failed",U),
POOL_STAT("Cancelled",U),POOL_STAT("Rejected",U),POOL_STAT("Initialized",B),POOL_STAT("Closed",B),POOL_STAT("Cancelling",B),
#undef POOL_STAT
