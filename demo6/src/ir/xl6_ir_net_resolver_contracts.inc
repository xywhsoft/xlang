#define LA "xl6NetAddressListDestroyV1"
#define RA "xl6NetAddressDestroyV1"
#define RO "xl6NetResolverDestroyV1"
#define RC "xl6NetResolverConfigDestroyV1"
#define RS "xl6NetResolverSnapshotDestroyV1"
#define RQ "xl6NetResolveOperationDestroyV1"
#define RF .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE
    {"xl6NetAddressListResolveV1",T,V,3,{S,I,I},.resultOwner=LA,RF},
    {"xl6NetAddressListDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={LA},.noRaise=1,.nativeHandlePolicy="xl6NetAddressListHandlePolicyV1Get"},
    {"xl6NetAddressListCountV1",T,I,1,{V},.parameterOwners={LA},.pure=1,.noRaise=1,.nonblocking=1},
    {"xl6NetAddressListAtV1",T,V,2,{V,I},.parameterOwners={LA},.resultOwner=RA,.nonblocking=1,RF},
    {"xl6NetAddressListFamilyV1",T,I,2,{V,I},.parameterOwners={LA},.pure=1,.nonblocking=1,RF},
    {"xl6NetAddressListPortV1",T,I,2,{V,I},.parameterOwners={LA},.pure=1,.nonblocking=1,RF},
    {"xl6NetAddressListTextV1",T,S,2,{V,I},.parameterOwners={LA},.nonblocking=1,RF},
    {"xl6NetAddressListCloneV1",T,V,1,{V},.parameterOwners={LA},.resultOwner=LA,.nonblocking=1,RF},
    {"xl6NetAddressListWithPortV1",T,V,2,{V,I},.parameterOwners={LA},.resultOwner=LA,.nonblocking=1,RF},
    {"xl6NetResolverConfigCreateV1",T,V,0,{0},.resultOwner=RC,.nonblocking=1,RF},
    {"xl6NetResolverConfigDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RC},.noRaise=1,.nativeHandlePolicy="xl6NetResolverConfigHandlePolicyV1Get"},
    {"xl6NetResolverConfigWorkersV1",T,I,1,{V},.parameterOwners={RC},.pure=1,.noRaise=1,.nonblocking=1},
    {"xl6NetResolverConfigWithWorkersV1",T,V,2,{V,I},.parameterOwners={RC},.resultOwner=RC,.nonblocking=1,RF},
#define XL6_NET_RESOLVER_CONFIG_SIZE(Field,Name,Minimum) \
    {"xl6NetResolverConfig" #Field "V1",T,U,1,{V},.parameterOwners={RC},.pure=1,.noRaise=1,.nonblocking=1}, \
    {"xl6NetResolverConfigWith" #Field "V1",T,V,2,{V,U},.parameterOwners={RC},.resultOwner=RC,.nonblocking=1,RF},
#include "src/runtime/xl6_net_resolver_config_fields.inc"
#undef XL6_NET_RESOLVER_CONFIG_SIZE
#define TTL(Field) \
    {"xl6NetResolverConfig" #Field "MillisV1",T,U,1,{V},.parameterOwners={RC},.pure=1,.noRaise=1,.nonblocking=1}, \
    {"xl6NetResolverConfigWith" #Field "MillisV1",T,V,2,{V,U},.parameterOwners={RC},.resultOwner=RC,.nonblocking=1,RF},
TTL(SuccessTTL)
TTL(FailureTTL)
#undef TTL
    {"xl6NetResolverSnapshotDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RS},.noRaise=1,.nativeHandlePolicy="xl6NetResolverSnapshotHandlePolicyV1Get"},
#define XL6_NET_RESOLVER_STAT(Field,Name) \
    {"xl6NetResolverSnapshot" #Field "V1",T,U,1,{V},.parameterOwners={RS},.pure=1,.noRaise=1,.nonblocking=1},
#include "src/runtime/xl6_net_resolver_stats_fields.inc"
#undef XL6_NET_RESOLVER_STAT
    {"xl6NetResolverCreateDefaultV1",T,V,0,{0},.resultOwner=RO,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,RF},
    {"xl6NetResolverCreateV1",T,V,1,{V},.parameterOwners={RC},.resultOwner=RO,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTION_SOURCE_V1,RF},
    {"xl6NetResolverDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RO},.noRaise=1,.nativeHandlePolicy="xl6NetResolverHandlePolicyV1Get"},
    {"xl6NetResolverCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RO},.nonblocking=1,RF},
    {"xl6NetResolverWaitV1",T,B,2,{V,I},.parameterOwners={RO},RF},
    {"xl6NetResolverRetryV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RO},.nonblocking=1,RF},
    {"xl6NetResolverClosedV1",T,B,1,{V},.parameterOwners={RO},.nonblocking=1,RF},
    {"xl6NetResolverClearV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RO},.nonblocking=1,RF},
    {"xl6NetResolverConfigurationV1",T,V,1,{V},.parameterOwners={RO},.resultOwner=RC,.nonblocking=1,RF},
    {"xl6NetResolverSnapshotV1",T,V,1,{V},.parameterOwners={RO},.resultOwner=RS,.nonblocking=1,RF},
    {"xl6NetResolverQueryV1",T,V,3,{V,S,I},.parameterOwners={RO},.resultOwner=RQ,.nonblocking=1,RF},
    {"xl6NetResolveOperationDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={RQ},.noRaise=1,.nativeHandlePolicy="xl6NetResolveOperationHandlePolicyV1Get"},
    {"xl6NetResolveOperationDoneV1",T,B,1,{V},.parameterOwners={RQ},.noRaise=1,.nonblocking=1},
    {"xl6NetResolveOperationCancelV1",T,B,1,{V},.parameterOwners={RQ},.nonblocking=1,RF},
    {"xl6NetResolveOperationWaitV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,1,{V},.parameterOwners={RQ},.resultFutureElementAbi=B,.nonblocking=1,RF},
    {"xl6NetResolveOperationResultV1",T,V,2,{V,I},.parameterOwners={RQ},.resultOwner=LA,.nonblocking=1,RF},
#undef RF
#undef RQ
#undef RS
#undef RC
#undef RO
#undef RA
#undef LA
