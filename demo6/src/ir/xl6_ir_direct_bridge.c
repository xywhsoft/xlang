/* Included by xl6_ir.c after the canonical ABI predicates. Both lowering and
 * verification consume these closed value/resource descriptors. Cgen receives
 * explicit native-field projections, never symbol-dependent algorithms. */
typedef struct XL6_IrNumericEnumMemberContract {
    const char* name;
    uint64 magnitude;
    int negative;
} XL6_IrNumericEnumMemberContract;
typedef struct XL6_IrNumericEnumContract {
    const char* declarationName;
    XL6_IrCAbi cAbi;
    XL6_IrIntegerTarget integerTarget;
    const XL6_IrNumericEnumMemberContract* members;
    size_t memberCount;
} XL6_IrNumericEnumContract;

/* Opt-in nominal scalar contracts, not a general permission to substitute
 * enums for integers. Canonical declaration/layout identity, representation
 * and the complete closed member vocabulary are checked before emission. */
static int xl6IrDirectNumericEnumMatches(const XL6_IrModule* module,
    XL6_IrTypeId id,const XL6_IrNumericEnumContract* expected)
{
    if(!module||!expected)return 0;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    if(!type||type->kind!=XL6_IR_TYPE_NUMERIC_ENUM||
        type->cAbi!=expected->cAbi||type->integerTarget!=expected->integerTarget||
        type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL||type->nullAbi!=XL6_IR_NULL_ABI_NONE||
        type->collectionAbi!=XL6_IR_COLLECTION_NONE||type->paramCount||type->returnCount||
        !type->nominalSymbol||!type->typeValueId||
        module->enumLayoutCount>module->enumLayoutCapacity||
        (module->enumLayoutCount&&!module->enumLayouts)||
        module->enumVariantCount>module->enumVariantCapacity||
        (module->enumVariantCount&&!module->enumVariants))return 0;
    const XL6_IrEnumLayout* layout=xl6IrEnumLayoutGet(module,id);
    const XL6_IrNode* declaration=layout?xl6IrNodeGet(module,layout->declaration):NULL;
    if(!layout||layout->type!=id||!declaration||declaration->kind!=XL6_IR_NODE_ENUM_DECL||
        declaration->sourceKind!=XL6_AST_ENUM||!(declaration->flags&XL6_AST_FLAG_NUMERIC_ENUM)||
        declaration->symbol!=type->nominalSymbol||!declaration->text||
        strcmp(declaration->text,expected->declarationName)||layout->variantCount!=expected->memberCount||
        layout->firstVariant>module->enumVariantCount||
        layout->variantCount>module->enumVariantCount-layout->firstVariant)return 0;
    for(size_t n=0;n<expected->memberCount;++n){
        const XL6_IrEnumVariant* item=&module->enumVariants[layout->firstVariant+n];
        const XL6_IrNode* member=xl6IrNodeGet(module,item->declaration);
        uint64 magnitude=0;int negative=0;
        if(item->ownerType!=id||item->payloadCount||!item->name||strcmp(item->name,expected->members[n].name)||
            !member||member->kind!=XL6_IR_NODE_ENUM_VARIANT||member->sourceKind!=XL6_AST_ENUM_VARIANT||
            !item->symbol||member->symbol!=item->symbol||item->tag!=n+1||
            !xl6IrContextualIntegerLiteralValue(module,item->valueExpression,id,&magnitude,&negative)||
            magnitude!=expected->members[n].magnitude||negative!=expected->members[n].negative)return 0;
    }
    return 1;
}
#include "xl6_ir_net_backend_enum.inc"
#include "xl6_ir_net_engine_state_enum.inc"
#include "xl6_ir_net_socket_enums.inc"
#include "xl6_ir_net_frame_status_enum.inc"
#include "xl6_ir_net_proxy_enums.inc"
#include "xl6_ir_tcp_options_enums.inc"
#include "xl6_ir_tcp_enums.inc"
#include "xl6_ir_tcp_server_enums.inc"
typedef struct XL6_IrDirectBridgeContract {
    const char* name;
    XL6_IrRuntimeBridgeKind kind;
    XL6_IrCAbi result;
    size_t parameterCount;
    XL6_IrCAbi parameters[8];
    XL6_IrCAbi resultArrayItemAbi;
    XL6_IrIntegerTarget resultIntegerTarget;
    XL6_IrCAbi parameterArrayItemAbis[8];
    const char* resultOwner;
    const char* parameterOwners[8];
    int nullableResult;
    int noRaise;
    XL6_IrNativeProductAbi resultProductAbi;
    /* Exact synchronous checked closure profiles. A closure pointer alone is
     * not a callable signature; zero means this parameter is not a closure. */
    XL6_IrCAbi parameterNullaryCallableResultAbis[8];
    /* Collections and dynamic borrows are sealed per entry, not inferred from
     * the common xvalue* C representation. Zero preserves the array profile. */
    XL6_IrCollectionAbi resultCollectionAbi;
    unsigned char parameterDynamicBorrows[8];
    unsigned char pure;
    unsigned char nonblocking;
    /* Future is a managed generic carrier, never an unchecked pointer or
     * builtin. Its resident producer must publish this exact element ABI. */
    XL6_IrCAbi resultFutureElementAbi;
    XL6_IrAsyncResultAbi resultFutureAbi;
    const char* nativeHandlePolicy;
    XL6_IrCallNativeArrayResultAbi nativeArrayResultAbi;
    XL6_IrCallErrorPhysicalAbi errorPhysicalAbi;
    /* Closed owning records are not PODs or arbitrary foreign structs. The
     * same permanent native-value profile authorizes argument borrows and
     * independent owned results; emission uses the ordinary call ABI. */
    XL6_IrNativeValueAbi resultNativeValueAbi;
    XL6_IrNativeValueAbi parameterNativeValueAbis[8];
    XL6_IrCallNativeContextAbi nativeContext;
    unsigned nativeServicesRequired;
    unsigned char genericFutureResult;
    /* One-based logical parameter index; zero means no slot-address input. */
    unsigned char genericValueParameter;
    unsigned char genericChannelParameter;
    unsigned char nativeClassArrayParameter;
    const char* nativeClassArrayOwner;
    unsigned char nativeVoidCallbackArrayParameter;
    XL6_IrCallNativeResultAbi nativeResultAbi;
    unsigned char managedHandleResult;
    unsigned char nativeClassParameter;
    const char* nativeClassOwner;
    /* Exact generic resource T, not just its common xvalue* representation. */
    unsigned char genericResourceResult;
    unsigned char genericResourceParameter;
    /* Nullary checked callable returns precisely this call's Future<T>.
     * Its one-based parameter selects a structural relation, not a pointer
     * ABI or a nominal class-name exception. No temporary resource for T. */
    unsigned char genericFutureStarterParameter;
    /* Nullary checked callback produces Future<T>'s exact T, directly into
     * its owned result slot, rather than returning an existing Future<T>. */
    unsigned char genericFutureValueCallbackParameter;
    /* Resident result stores a Sequence representation, not a Future<T>'s
     * logical invocation representation. This selects the shared codec plan. */
    unsigned char sequenceValueResult;
    const XL6_IrNumericEnumContract* resultEnum;
    const XL6_IrNumericEnumContract* parameterEnums[8];
    const XL6_IrNumericEnumContract* resultProductEnums[3];
} XL6_IrDirectBridgeContract;

static const XL6_IrDirectBridgeContract xl6IrDirectBridgeContracts[] = {
    {"xl6_xlang_context_destroy",XL6_IR_RUNTIME_BRIDGE_XLANG,XL6_IR_C_ABI_VOID,1,
        {XL6_IR_C_ABI_RUNTIME_XVALUE_PTR},.parameterOwners={"xl6_xlang_context_destroy"},.noRaise=1,
        .nativeHandlePolicy="xl6XlangContextHandlePolicyV1Get"},
#define S XL6_IR_C_ABI_MANAGED_STRING
#define I XL6_IR_C_ABI_I64
#define T XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE
#define B XL6_IR_C_ABI_BOOL
#define C XL6_IR_C_ABI_CHAR_U32
#define V XL6_IR_C_ABI_RUNTIME_XVALUE_PTR
#define Y XL6_IR_C_ABI_MANAGED_BYTES
#define F XL6_IR_C_ABI_F64
#define U XL6_IR_C_ABI_U64
#define M XL6_IR_C_ABI_TIME_I64
#include "xl6_ir_atomic_contracts.inc"
#include "xl6_ir_net_address_contracts.inc"
#include "xl6_ir_net_resolver_contracts.inc"
#include "xl6_ir_net_buffer_contracts.inc"
#include "xl6_ir_net_proxy_contracts.inc"
#include "xl6_ir_tcp_options_contracts.inc"
#include "xl6_ir_tcp_contracts.inc"
#include "xl6_ir_tcp_server_contracts.inc"
#include "xl6_ir_tcp_proxy_contracts.inc"
#include "xl6_ir_tcp_data_contracts.inc"
#include "xl6_ir_net_interfaces_contracts.inc"
#include "xl6_ir_net_backends_contracts.inc"
#include "xl6_ir_concurrent_contracts.inc"
#include "xl6_ir_executor_contracts.inc"
#include "xl6_ir_net_engine_contracts.inc"
#include "xl6_ir_net_socket_contracts.inc"
#include "xl6_ir_net_datagram_direct_contracts.inc"
#include "xl6_ir_net_datagram_error_contracts.inc"
#include "xl6_ir_net_batch_contracts.inc"
#include "xl6_ir_task_pool_contracts.inc"
#include "xl6_ir_channel_case_contracts.inc"
#define XL6_GENERIC_CREATE(Name,Owner) {Name,T,V,0,{0},.resultOwner=Owner,.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1}
#define XL6_GENERIC_CREATE_PARENT(Name,Owner) {Name,T,V,1,{V},.resultOwner=Owner,.parameterOwners={"xl6CancellationTokenDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1}
#define XL6_GENERIC_DESTROY(Name,Owner) {Name,T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={Owner},.nativeHandlePolicy="xl6PromiseHandlePolicyV1Get",.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE}
#define XL6_GENERIC_FUTURE(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,1,{V},.parameterOwners={Owner},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1,.genericFutureResult=1}
#define XL6_GENERIC_RESOLVE(Name,Owner) {Name,T,B,2,{V,0},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1,.genericValueParameter=2}
#define XL6_GENERIC_REJECT(Name,Owner) {Name,T,B,2,{V,XL6_IR_C_ABI_RUNTIME_ERROR_PTR},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE}
#define XL6_GENERIC_CANCEL(Name,Owner) {Name,T,B,1,{V},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE}
#define XL6_GENERIC_CLOSE(Name,Owner) XL6_GENERIC_CANCEL(Name,Owner)
#define XL6_GENERIC_DONE(Name,Owner) {Name,T,B,1,{V},.parameterOwners={Owner},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE}
#define XL6_GENERIC_REQUESTED(Name,Owner) XL6_GENERIC_DONE(Name,Owner)
#define XL6_GENERIC_TASK_GROUP_START(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,XL6_IR_C_ABI_CLOSURE_PTR},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1,.genericFutureResult=1,.genericFutureStarterParameter=2}
#define XL6_GENERIC_TASK_POOL_SUBMIT(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,XL6_IR_C_ABI_CLOSURE_PTR},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1,.genericFutureResult=1,.genericFutureValueCallbackParameter=2}
#define XL6_GENERIC_TASK_POOL_SUBMIT_CANCEL(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,3,{V,XL6_IR_C_ABI_CLOSURE_PTR,V},.parameterOwners={Owner,NULL,"xl6CancellationTokenDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1,.genericFutureResult=1,.genericFutureValueCallbackParameter=2}
#define XL6_GENERIC_CHANNEL_CASE_RECEIVE(Name,Owner) {Name,T,V,1,{V},.resultOwner=Owner,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1,.genericChannelParameter=1}
#define XL6_GENERIC_CHANNEL_CASE_SEND(Name,Owner) {Name,T,V,2,{V,0},.resultOwner=Owner,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1,.genericChannelParameter=1,.genericValueParameter=2}
#define XL6_GENERIC_CHANNEL_SELECTION_VALUE(Name,Owner) {Name,T,0,3,{V,V,V},.parameterOwners={Owner,"xl6ChannelCaseDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1,.genericChannelParameter=3,.nativeResultAbi=XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1}
#define XL6_GENERIC_CHANNEL_SEND_CREATE(Name,Owner) {Name,T,V,2,{V,0},.resultOwner=Owner,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1,.genericChannelParameter=1,.genericValueParameter=2,.genericResourceResult=1}
#define XL6_GENERIC_CHANNEL_RECEIVE_CREATE(Name,Owner) {Name,T,V,1,{V},.resultOwner=Owner,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1,.genericChannelParameter=1,.genericResourceResult=1}
#define XL6_GENERIC_CHANNEL_SEND_BUILDER_DESTROY(Name,Owner) {Name,T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeHandlePolicy="xl6ChannelSendFutureBuilderHandlePolicyV1Get"}
#define XL6_GENERIC_CHANNEL_RECEIVE_BUILDER_DESTROY(Name,Owner) {Name,T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeHandlePolicy="xl6ChannelReceiveFutureBuilderHandlePolicyV1Get"}
#define XL6_GENERIC_CHANNEL_RECEIVE_DESTROY(Name,Owner) {Name,T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeHandlePolicy="xl6ChannelReceiveResultHandlePolicyV1Get"}
#define XL6_GENERIC_CHANNEL_SEND_START(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,1,{V},.parameterOwners={Owner},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,.resultFutureElementAbi=I,.genericResourceParameter=1}
#define XL6_GENERIC_CHANNEL_SEND_START_CANCEL(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,V},.parameterOwners={Owner,"xl6CancellationTokenDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,.resultFutureElementAbi=I,.genericResourceParameter=1}
#define XL6_GENERIC_CHANNEL_RECEIVE_HANDLE(Name,Owner) {Name,T,V,1,{V},.resultOwner=Owner,.parameterOwners={"xl6ChannelReceiveFutureBuilderDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.managedHandleResult=1,.genericResourceResult=1,.genericResourceParameter=1}
#define XL6_GENERIC_CHANNEL_RECEIVE_STATUS(Name,Owner) {Name,T,I,1,{V},.parameterOwners={Owner},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE}
#define XL6_GENERIC_CHANNEL_RECEIVE_VALUE(Name,Owner) {Name,T,0,1,{V},.parameterOwners={Owner},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1,.nativeResultAbi=XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1,.sequenceValueResult=1}
#define XL6_GENERIC_CHANNEL_RECEIVE_START(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,V},.parameterOwners={Owner},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,.nativeClassParameter=2,.nativeClassOwner="xl6ChannelReceiveResultDestroyV1",.genericFutureResult=1,.genericResourceParameter=1}
#define XL6_GENERIC_CHANNEL_RECEIVE_START_CANCEL(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,3,{V,V,V},.parameterOwners={Owner,NULL,"xl6CancellationTokenDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1,.nativeClassParameter=2,.nativeClassOwner="xl6ChannelReceiveResultDestroyV1",.genericFutureResult=1,.genericResourceParameter=1}
#define XL6_NATIVE_GENERIC_BRIDGE(Name,Owner,Role) XL6_GENERIC_##Role(Name,Owner),
#define XL6_GENERIC_CHANNEL_DEADLINE_SEND_FOR(Name,Owner) {Name,T,V,3,{V,0,I},.resultOwner=Owner,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1,.genericChannelParameter=1,.genericValueParameter=2,.genericResourceResult=1}
#define XL6_GENERIC_CHANNEL_DEADLINE_SEND_UNTIL(Name,Owner) {Name,T,V,3,{V,0,U},.resultOwner=Owner,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1,.genericChannelParameter=1,.genericValueParameter=2,.genericResourceResult=1}
#define XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_FOR(Name,Owner) {Name,T,V,2,{V,I},.resultOwner=Owner,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1,.genericChannelParameter=1,.genericResourceResult=1}
#define XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_UNTIL(Name,Owner) {Name,T,V,2,{V,U},.resultOwner=Owner,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1,.genericChannelParameter=1,.genericResourceResult=1}
#define XL6_GENERIC_CHANNEL_DEADLINE_SEND_DESTROY(Name,Owner) {Name,T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeHandlePolicy="xl6ChannelDeadlineSendBuilderHandlePolicyV1Get"}
#define XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_DESTROY(Name,Owner) {Name,T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={Owner},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeHandlePolicy="xl6ChannelDeadlineReceiveBuilderHandlePolicyV1Get"}
#define XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_HANDLE(Name,Owner) {Name,T,V,1,{V},.resultOwner=Owner,.parameterOwners={"xl6ChannelDeadlineReceiveBuilderDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.managedHandleResult=1,.genericResourceResult=1,.genericResourceParameter=1}
#define XL6_FINITE_SERVICES (XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1|XL6_IR_CALL_NATIVE_SERVICE_DEADLINE_V1)
#define XL6_GENERIC_CHANNEL_DEADLINE_SEND_START(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,1,{V},.parameterOwners={Owner},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeServicesRequired=XL6_FINITE_SERVICES,.resultFutureElementAbi=I,.genericResourceParameter=1}
#define XL6_GENERIC_CHANNEL_DEADLINE_SEND_START_CANCEL(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,V},.parameterOwners={Owner,"xl6CancellationTokenDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeServicesRequired=XL6_FINITE_SERVICES,.resultFutureElementAbi=I,.genericResourceParameter=1}
#define XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_START(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,2,{V,V},.parameterOwners={Owner},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeServicesRequired=XL6_FINITE_SERVICES,.nativeClassParameter=2,.nativeClassOwner="xl6ChannelReceiveResultDestroyV1",.genericFutureResult=1,.genericResourceParameter=1}
#define XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_START_CANCEL(Name,Owner) {Name,T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,3,{V,V,V},.parameterOwners={Owner,NULL,"xl6CancellationTokenDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE,.nativeContext=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1,.nativeServicesRequired=XL6_FINITE_SERVICES,.nativeClassParameter=2,.nativeClassOwner="xl6ChannelReceiveResultDestroyV1",.genericFutureResult=1,.genericResourceParameter=1}
#include "src/abi/xl6_native_generic_contracts.inc"
#undef XL6_NATIVE_GENERIC_BRIDGE
#undef XL6_GENERIC_TASK_GROUP_START
#undef XL6_GENERIC_TASK_POOL_SUBMIT
#undef XL6_GENERIC_TASK_POOL_SUBMIT_CANCEL
#undef XL6_GENERIC_CHANNEL_DEADLINE_SEND_FOR
#undef XL6_GENERIC_CHANNEL_DEADLINE_SEND_UNTIL
#undef XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_FOR
#undef XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_UNTIL
#undef XL6_GENERIC_CHANNEL_DEADLINE_SEND_DESTROY
#undef XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_DESTROY
#undef XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_HANDLE
#undef XL6_GENERIC_CHANNEL_DEADLINE_SEND_START
#undef XL6_GENERIC_CHANNEL_DEADLINE_SEND_START_CANCEL
#undef XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_START
#undef XL6_GENERIC_CHANNEL_DEADLINE_RECEIVE_START_CANCEL
#undef XL6_FINITE_SERVICES
#undef XL6_GENERIC_CREATE
#undef XL6_GENERIC_CREATE_PARENT
#undef XL6_GENERIC_DESTROY
#undef XL6_GENERIC_FUTURE
#undef XL6_GENERIC_RESOLVE
#undef XL6_GENERIC_REJECT
#undef XL6_GENERIC_CANCEL
#undef XL6_GENERIC_CLOSE
#undef XL6_GENERIC_DONE
#undef XL6_GENERIC_REQUESTED
#undef XL6_GENERIC_CHANNEL_CASE_RECEIVE
#undef XL6_GENERIC_CHANNEL_CASE_SEND
#undef XL6_GENERIC_CHANNEL_SELECTION_VALUE
#undef XL6_GENERIC_CHANNEL_SEND_CREATE
#undef XL6_GENERIC_CHANNEL_RECEIVE_CREATE
#undef XL6_GENERIC_CHANNEL_SEND_BUILDER_DESTROY
#undef XL6_GENERIC_CHANNEL_RECEIVE_BUILDER_DESTROY
#undef XL6_GENERIC_CHANNEL_RECEIVE_DESTROY
#undef XL6_GENERIC_CHANNEL_SEND_START
#undef XL6_GENERIC_CHANNEL_SEND_START_CANCEL
#undef XL6_GENERIC_CHANNEL_RECEIVE_HANDLE
#undef XL6_GENERIC_CHANNEL_RECEIVE_STATUS
#undef XL6_GENERIC_CHANNEL_RECEIVE_VALUE
#undef XL6_GENERIC_CHANNEL_RECEIVE_START
#undef XL6_GENERIC_CHANNEL_RECEIVE_START_CANCEL
#include "xl6_ir_asn1_contracts.inc"
#include "xl6_ir_crypto_contracts.inc"
#include "xl6_ir_compress_contracts.inc"
#include "xl6_ir_template_contracts.inc"
    {"xl6Crc32StringV1",T,I,3,{U,S,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_32,.pure=1,.nonblocking=1},
    {"xl6CodecHexTextV1",T,S,3,{S,I,I},.pure=1,.nonblocking=1},
    {"xl6CodecHexBytesV1",T,S,3,{Y,I,I},.pure=1,.nonblocking=1},
    {"xl6CodecHexDecodeV1",T,Y,3,{S,I,I},.pure=1,.nonblocking=1},
    {"xl6CodecBase64TextV1",T,S,5,{S,I,I,S,I},.pure=1,.nonblocking=1},
    {"xl6CodecBase64BytesV1",T,S,5,{Y,I,I,S,I},.pure=1,.nonblocking=1},
    {"xl6CodecBase64DecodeV1",T,Y,5,{S,I,I,S,I},.pure=1,.nonblocking=1},
    {"xl6CodecPercentTextV1",T,S,6,{S,I,S,B,B,I},.pure=1,.nonblocking=1},
    {"xl6CodecPercentBytesV1",T,S,6,{Y,I,S,B,B,I},.pure=1,.nonblocking=1},
    {"xl6CodecPercentDecodeV1",T,Y,3,{S,I,B},.pure=1,.nonblocking=1},
    {"xl6Crc32BytesV1",T,I,3,{U,Y,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_32,.pure=1,.nonblocking=1},
    {"xl6Crc32BufferV1",T,I,3,{U,V,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_32,.parameterOwners={NULL,"xl6BufferDestroyV1"},.nonblocking=1},
    {"xl6DictValueRemoveV1",T,B,2,{V,S},.parameterDynamicBorrows={1,0}},
    /* Printing borrows one dynamic argument array and can allocate or fail I/O.
     * Checked calls propagate the original thread error before the language
     * wrapper considers its boolean-only I/O fallback. */
    {"xl6_xrt_print",XL6_IR_RUNTIME_BRIDGE_PRINT,B,2,{V,B},.parameterDynamicBorrows={1,0}},
    {"xl6_xrt_dynamic_fields_create",T,V,0,{0},.resultOwner="xl6_xrt_dynamic_fields_destroy"},
    {"xl6_xrt_dynamic_fields_wrap",T,V,1,{XL6_IR_C_ABI_RAW_POINTER},.resultOwner="xl6_xrt_dynamic_fields_destroy"},
    {"xl6_xrt_dynamic_fields_from_value",T,V,1,{V},.resultOwner="xl6_xrt_dynamic_fields_destroy",.parameterDynamicBorrows={1},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_destroy",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"},.nativeHandlePolicy="xl6DynamicFieldsHandlePolicyV1Get",.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_native_handle",T,XL6_IR_C_ABI_RAW_POINTER,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"}},
    {"xl6_xrt_dynamic_fields_count",T,I,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"}},
    {"xl6_xrt_dynamic_fields_capacity",T,I,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"}},
    {"xl6_xrt_dynamic_fields_reserve",T,B,2,{V,I},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"}},
    {"xl6_xrt_dynamic_fields_trim",T,B,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"}},
    {"xl6_xrt_dynamic_fields_clear",T,B,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_has",T,B,2,{V,S},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"}},
    {"xl6_xrt_dynamic_fields_get",T,V,2,{V,S},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"}},
    {"xl6_xrt_dynamic_fields_copy",T,V,2,{V,S},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_set",T,B,3,{V,S,V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"},.parameterDynamicBorrows={0,0,1},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_remove",T,B,2,{V,S},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_take",T,V,2,{V,S},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"}},
    {"xl6_xrt_dynamic_fields_keys",T,V,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"}},
    {"xl6_xrt_dynamic_fields_values",T,V,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_items",T,V,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_to_value",T,V,1,{V},.parameterOwners={"xl6_xrt_dynamic_fields_destroy"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_merge",T,B,3,{V,V,B},.parameterOwners={"xl6_xrt_dynamic_fields_destroy","xl6_xrt_dynamic_fields_destroy"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_dynamic_fields_clone",T,V,1,{V},.resultOwner="xl6_xrt_dynamic_fields_destroy",.parameterOwners={"xl6_xrt_dynamic_fields_destroy"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6BufferCreateV1",T,V,0,{0},.resultOwner="xl6BufferDestroyV1"},
    {"xl6BufferCreateReserveV1",T,V,1,{I},.resultOwner="xl6BufferDestroyV1"},
    {"xl6BufferAdoptV1",T,V,1,{XL6_IR_C_ABI_RAW_POINTER},.resultOwner="xl6BufferDestroyV1"},
    {"xl6BufferDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6BufferDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6BufferHandlePolicyV1Get"},
    {"xl6BufferLengthV1",T,I,1,{V},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferCapacityV1",T,I,1,{V},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferReserveV1",T,B,2,{V,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferClearV1",T,B,1,{V},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferAppendRawV1",T,B,3,{V,XL6_IR_C_ABI_RAW_POINTER,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferInsertRawV1",T,B,4,{V,I,XL6_IR_C_ABI_RAW_POINTER,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferAppendByteV1",T,B,2,{V,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferByteAtV1",T,I,2,{V,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_8,.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferSetByteV1",T,B,3,{V,I,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferCopyFromV1",T,B,5,{V,I,V,I,I},.parameterOwners={"xl6BufferDestroyV1",NULL,"xl6BufferDestroyV1"}},
    {"xl6BufferCopyWithinV1",T,B,4,{V,I,I,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferFillV1",T,B,4,{V,I,I,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferCompareV1",T,I,4,{V,V,I,I},.parameterOwners={"xl6BufferDestroyV1","xl6BufferDestroyV1"}},
    {"xl6BufferIndexOfV1",T,I,5,{V,V,I,I,I},.parameterOwners={"xl6BufferDestroyV1","xl6BufferDestroyV1"}},
    {"xl6BufferDataV1",T,XL6_IR_C_ABI_RAW_POINTER,1,{V},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferHandleV1",T,XL6_IR_C_ABI_RAW_POINTER,1,{V},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferFromTextV1",T,V,2,{S,I},.resultOwner="xl6BufferDestroyV1"},
    {"xl6BufferFromBytesV1",T,V,2,{Y,I},.resultOwner="xl6BufferDestroyV1"},
    {"xl6BufferCopyV1",T,V,2,{V,I},.resultOwner="xl6BufferDestroyV1",.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferFromHexV1",T,V,3,{S,I,I},.resultOwner="xl6BufferDestroyV1"},
    {"xl6BufferHexV1",T,S,3,{V,I,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferFromBase64V1",T,V,5,{S,I,S,I,I},.resultOwner="xl6BufferDestroyV1"},
    {"xl6BufferBase64V1",T,S,5,{V,I,S,I,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferResizeV1",T,B,3,{V,I,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferTrimV1",T,B,1,{V},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferAssignTextV1",T,B,3,{V,S,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferAssignBytesV1",T,B,3,{V,Y,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferAssignBufferV1",T,B,3,{V,V,I},.parameterOwners={"xl6BufferDestroyV1","xl6BufferDestroyV1"}},
    {"xl6BufferAppendTextV1",T,B,3,{V,S,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferAppendBytesV1",T,B,3,{V,Y,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferAppendBufferV1",T,B,3,{V,V,I},.parameterOwners={"xl6BufferDestroyV1","xl6BufferDestroyV1"}},
    {"xl6BufferInsertTextV1",T,B,4,{V,I,S,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferInsertBytesV1",T,B,4,{V,I,Y,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferInsertBufferV1",T,B,4,{V,I,V,I},.parameterOwners={"xl6BufferDestroyV1",NULL,"xl6BufferDestroyV1"}},
    {"xl6BufferRemoveV1",T,B,3,{V,I,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferBytesV1",T,Y,2,{V,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferTextV1",T,S,2,{V,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferIteratorCreateV1",T,V,2,{V,I},.resultOwner="xl6BufferIteratorDestroyV1",.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6BufferIteratorDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6BufferIteratorDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6BufferIteratorHandlePolicyV1Get"},
    {"xl6BufferIteratorHasNextV1",T,B,1,{V},.parameterOwners={"xl6BufferIteratorDestroyV1"}},
    {"xl6BufferIteratorNextV1",T,I,1,{V},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_8,.parameterOwners={"xl6BufferIteratorDestroyV1"}},
    {"xl6BufferIteratorCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6BufferIteratorDestroyV1"}},
    {"xl6BufferIteratorClosedV1",T,B,1,{V},.parameterOwners={"xl6BufferIteratorDestroyV1"}},
    {"xl6ProcessPipelineCreateV1",T,V,0,{0},.resultOwner="xl6ProcessPipelineDestroyV1"},
    {"xl6ProcessPipelineDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6ProcessPipelineDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6ProcessPipelineHandlePolicyV1Get"},
    {"xl6ProcessPipelineAddV1",T,XL6_IR_C_ABI_VOID,2,{V,V},.parameterOwners={"xl6ProcessPipelineDestroyV1","xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessPipelineClearV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6ProcessPipelineDestroyV1"}},
    {"xl6ProcessPipelineCountV1",T,I,1,{V},.parameterOwners={"xl6ProcessPipelineDestroyV1"}},
    {"xl6ProcessPipelineRunV1",T,V,4,{V,V,Y,I},.resultOwner="xl6PipelineResultDestroyV1",.parameterOwners={"xl6ProcessPipelineDestroyV1","xl6ProcessRunOptionsDestroyV1"}},
    {"xl6PipelineResultDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6PipelineResultHandlePolicyV1Get"},
    {"xl6PipelineResultSuccessV1",T,B,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultTimedOutV1",T,B,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultCancelledV1",T,B,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultWaitV1",T,I,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultCountV1",T,I,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultCompletedV1",T,I,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultFailedIndexV1",T,I,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultExitCodeV1",T,I,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultInputWrittenV1",T,U,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultDurationV1",T,U,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultStdoutTruncatedV1",T,B,1,{V},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultStdoutV1",T,Y,2,{V,I},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultStdoutTextV1",T,S,2,{V,I},.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6PipelineResultStageV1",T,V,2,{V,I},.resultOwner="xl6ProcessStageDestroyV1",.parameterOwners={"xl6PipelineResultDestroyV1"}},
    {"xl6ProcessStageDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6ProcessStageDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6ProcessStageHandlePolicyV1Get"},
    {"xl6ProcessStageSuccessV1",T,B,1,{V},.parameterOwners={"xl6ProcessStageDestroyV1"}},
    {"xl6ProcessStageExitCodeV1",T,I,1,{V},.parameterOwners={"xl6ProcessStageDestroyV1"}},
    {"xl6ProcessStageStderrTruncatedV1",T,B,1,{V},.parameterOwners={"xl6ProcessStageDestroyV1"}},
    {"xl6ProcessStageStderrV1",T,Y,2,{V,I},.parameterOwners={"xl6ProcessStageDestroyV1"}},
    {"xl6ProcessStageStderrTextV1",T,S,2,{V,I},.parameterOwners={"xl6ProcessStageDestroyV1"}},
    {"xl6ProcessStageStatusV1",T,V,1,{V},.resultOwner="xl6ProcessStatusDestroyV1",.parameterOwners={"xl6ProcessStageDestroyV1"}},
    {"xl6ProcessOpenV1",T,B,2,{S,I}},
    {"xl6ProcessSpawnV1",T,V,1,{V},.resultOwner="xl6ProcessDestroyV1",.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6ProcessDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6ProcessHandlePolicyV1Get"},
    {"xl6ProcessIdV1",T,U,1,{V},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessStateV1",T,I,1,{V},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessRunningV1",T,B,1,{V},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessCloseV1",T,B,2,{V,I},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessResizeV1",T,B,3,{V,I,I},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessInterruptV1",T,B,1,{V},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessTerminateV1",T,B,1,{V},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessKillV1",T,B,1,{V},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessKillTreeV1",T,B,1,{V},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessWaitReadyV1",T,B,2,{V,I},.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessWaitReadyAsyncV1",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,1,{V},.parameterOwners={"xl6ProcessDestroyV1"},.nonblocking=1,.resultFutureElementAbi=B},
    {"xl6ProcessLiveStatusV1",T,V,1,{V},.resultOwner="xl6ProcessStatusDestroyV1",.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessReaderV1",T,V,2,{V,I},.resultOwner="xl6_xrt_io_reader_destroy",.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessWriterV1",T,V,1,{V},.resultOwner="xl6_xrt_io_writer_destroy",.parameterOwners={"xl6ProcessDestroyV1"}},
    {"xl6ProcessTerminalSupportedV1",T,B,0,{0},.noRaise=1},
    {"xl6ProcessCommandShellV1",T,V,2,{S,I},.resultOwner="xl6ProcessCommandDestroyV1"},
    {"xl6ProcessCommandDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6ProcessCommandDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6ProcessCommandHandlePolicyV1Get"},
    {"xl6ProcessCommandWorkDirV1",T,XL6_IR_C_ABI_VOID,3,{V,S,I},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandArg0V1",T,XL6_IR_C_ABI_VOID,3,{V,S,I},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandRemoveEnvV1",T,XL6_IR_C_ABI_VOID,3,{V,S,I},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandEnvV1",T,XL6_IR_C_ABI_VOID,5,{V,S,S,I,I},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandClearEnvV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandInheritEnvV1",T,XL6_IR_C_ABI_VOID,2,{V,B},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandNewConsoleV1",T,XL6_IR_C_ABI_VOID,2,{V,B},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandProcessGroupV1",T,XL6_IR_C_ABI_VOID,2,{V,B},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandHideWindowV1",T,XL6_IR_C_ABI_VOID,2,{V,B},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandTerminalV1",T,XL6_IR_C_ABI_VOID,2,{V,B},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandTerminalSizeV1",T,XL6_IR_C_ABI_VOID,3,{V,I,I},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandStdioV1",T,XL6_IR_C_ABI_VOID,3,{V,I,I},.parameterOwners={"xl6ProcessCommandDestroyV1"}},
    {"xl6ProcessCommandFileV1",T,XL6_IR_C_ABI_VOID,3,{V,I,V},.parameterOwners={"xl6ProcessCommandDestroyV1",NULL,"xl6_xrt_fs_file_destroy"}},
    {"xl6ProcessRunOptionsCreateV1",T,V,6,{I,I,I,I,I,U},.resultOwner="xl6ProcessRunOptionsDestroyV1"},
    {"xl6ProcessRunOptionsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6ProcessRunOptionsDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6ProcessRunOptionsHandlePolicyV1Get"},
    {"xl6ProcessRunOptionsCancelV1",T,B,1,{V},.parameterOwners={"xl6ProcessRunOptionsDestroyV1"}},
    {"xl6ProcessRunOptionsRequestedV1",T,B,1,{V},.parameterOwners={"xl6ProcessRunOptionsDestroyV1"}},
    {"xl6ProcessRunV1",T,V,4,{V,V,Y,I},.resultOwner="xl6ProcessResultDestroyV1",.parameterOwners={"xl6ProcessCommandDestroyV1","xl6ProcessRunOptionsDestroyV1"}},
    {"xl6ProcessResultDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6ProcessResultHandlePolicyV1Get"},
    {"xl6ProcessResultSuccessV1",T,B,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultTimedOutV1",T,B,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultCancelledV1",T,B,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultWaitV1",T,I,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultInputWrittenV1",T,U,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultDurationV1",T,U,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultStdoutTruncatedV1",T,B,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultStderrTruncatedV1",T,B,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultExitCodeV1",T,I,1,{V},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultStdoutV1",T,Y,2,{V,I},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultStderrV1",T,Y,2,{V,I},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultStdoutTextV1",T,S,2,{V,I},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultStderrTextV1",T,S,2,{V,I},.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessResultStatusV1",T,V,1,{V},.resultOwner="xl6ProcessStatusDestroyV1",.parameterOwners={"xl6ProcessResultDestroyV1"}},
    {"xl6ProcessStatusDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6ProcessStatusDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6ProcessStatusHandlePolicyV1Get"},
    {"xl6ProcessStatusKindV1",T,I,1,{V},.parameterOwners={"xl6ProcessStatusDestroyV1"}},
    {"xl6ProcessStatusCodeV1",T,I,1,{V},.parameterOwners={"xl6ProcessStatusDestroyV1"}},
    {"xl6ProcessStatusSignalV1",T,I,1,{V},.parameterOwners={"xl6ProcessStatusDestroyV1"}},
    {"xl6ProcessStatusStopV1",T,I,1,{V},.parameterOwners={"xl6ProcessStatusDestroyV1"}},
    {"xl6ProcessStatusCoreDumpedV1",T,B,1,{V},.parameterOwners={"xl6ProcessStatusDestroyV1"}},
    {"xl6SignalSupportedV1",T,B,1,{I}},
    {"xl6SignalNameV1",T,S,1,{I}},
    {"xl6SignalIgnoreV1",T,B,1,{I}},
    {"xl6SignalRestoreV1",T,B,1,{I}},
    {"xl6SignalRestoreAllV1",T,B,0,{0}},
    {"xl6SignalRaiseV1",T,B,1,{I}},
    {"xl6SignalCountV1",T,U,1,{I}},
    {"xl6SignalReceivedV1",T,B,1,{I}},
    {"xl6SignalClearV1",T,XL6_IR_C_ABI_VOID,1,{I}},
    {"xl6SignalHealthyV1",T,B,0,{0}},
    {"xl6SignalResetV1",T,XL6_IR_C_ABI_VOID,0,{0}},
    {"xl6SignalQueueCreateV1",T,V,3,{I,B,I},.resultOwner="xl6SignalQueueDestroyV1"},
    {"xl6SignalQueueDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6SignalQueueHandlePolicyV1Get"},
    {"xl6SignalQueueCheckOwnerV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueCloseV1",T,B,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueActiveV1",T,B,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueCodeV1",T,I,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueuePendingV1",T,I,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueClosedV1",T,B,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueuePrepareV1",T,B,2,{V,I},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueSnapshotV1",T,V,1,{V},.resultOwner="xl6SignalEventDestroyV1",.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalQueueConsumeV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalQueueDestroyV1"}},
    {"xl6SignalEventDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6SignalEventHandlePolicyV1Get"},
    {"xl6SignalEventCodeV1",T,I,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventSystemCodeV1",T,I,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventCountV1",T,U,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventTotalV1",T,U,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventTimeV1",T,M,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventCoalescedV1",T,B,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventSaturatedV1",T,B,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6SignalEventNameV1",T,S,1,{V},.parameterOwners={"xl6SignalEventDestroyV1"}},
    {"xl6TaskGroupCreateLanguageV1",T,V,2,{I,I},.resultOwner="xl6SignalTaskGroupDestroyV1",.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupCreateCancelLanguageV1",T,V,3,{V,I,I},.resultOwner="xl6SignalTaskGroupDestroyV1",.parameterOwners={"xl6CancellationTokenDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupChildLanguageV1",T,V,3,{V,I,I},.resultOwner="xl6SignalTaskGroupDestroyV1",.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupValidLanguageV1",T,B,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupCountLanguageV1",T,U,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupReapLanguageV1",T,U,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupTokenLanguageV1",T,V,1,{V},.resultOwner="xl6CancellationTokenDestroyV1",.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupThrowLanguageV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotLanguageV1",T,V,1,{V},.resultOwner="xl6TaskGroupSnapshotDestroyV1",.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6TaskGroupSnapshotHandlePolicyV1Get"},
    {"xl6TaskGroupUntilLanguageV1",T,B,2,{V,U},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupTimeoutLanguageV1",T,B,2,{V,I},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupWaitCancelLanguageV1",T,B,2,{V,V},.parameterOwners={"xl6SignalTaskGroupDestroyV1","xl6CancellationTokenDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupUntilCancelLanguageV1",T,B,3,{V,U,V},.parameterOwners={"xl6SignalTaskGroupDestroyV1",NULL,"xl6CancellationTokenDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupTimeoutCancelLanguageV1",T,B,3,{V,I,V},.parameterOwners={"xl6SignalTaskGroupDestroyV1",NULL,"xl6CancellationTokenDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotActiveV1",T,U,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotAddedV1",T,U,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotCompletedV1",T,U,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotSucceededV1",T,U,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotFailedV1",T,U,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotCancelledV1",T,U,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotClosedV1",T,U,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotRejectedV1",T,U,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotFirstIndexV1",T,U,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotFirstStateV1",T,I,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotHasFirstV1",T,B,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotAcceptingV1",T,B,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6TaskGroupSnapshotCancellingV1",T,B,1,{V},.parameterOwners={"xl6TaskGroupSnapshotDestroyV1"},.nonblocking=1,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6SignalTaskGroupCreateV1",T,V,0,{0},.resultOwner="xl6SignalTaskGroupDestroyV1",.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6SignalTaskGroupDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6TaskGroupHandlePolicyV1Get"},
    {"xl6SignalTaskGroupCloseV1",T,B,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6SignalTaskGroupCancelV1",T,B,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6_xrt_task_group_wrap",T,V,1,{XL6_IR_C_ABI_RAW_POINTER},.resultOwner="xl6SignalTaskGroupDestroyV1"},
    {"xl6_xrt_task_group_child",T,XL6_IR_C_ABI_RAW_POINTER,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_task_group_add",T,B,2,{V,V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.parameterDynamicBorrows={0,1},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_task_group_bind_parent",T,B,2,{V,V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"},.parameterDynamicBorrows={0,1},.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6_xrt_task_group_count",T,XL6_IR_C_ABI_C_INT,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6_xrt_task_group_reap",T,XL6_IR_C_ABI_C_INT,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6_xrt_task_group_join",T,B,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6_xrt_task_group_join_timeout",T,B,2,{V,I},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6_xrt_task_group_join_until",T,B,2,{V,I},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6_xrt_task_group_wait",T,B,1,{V},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6_xrt_task_group_wait_timeout",T,B,2,{V,I},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6_xrt_task_group_wait_until",T,B,2,{V,I},.parameterOwners={"xl6SignalTaskGroupDestroyV1"}},
    {"xl6_xrt_task_group_join_async",T,XL6_IR_C_ABI_RUNTIME_FUTURE_PTR,1,{V},
        .parameterOwners={"xl6SignalTaskGroupDestroyV1"},.resultFutureAbi=XL6_IR_ASYNC_RESULT_VOID,.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE},
    {"xl6EnvGetV1",T,S,2,{S,I},.nullableResult=1},
    {"xl6EnvHasV1",T,B,2,{S,I}},
    {"xl6EnvSetV1",T,XL6_IR_C_ABI_VOID,4,{S,S,I,I}},
    {"xl6EnvRemoveV1",T,XL6_IR_C_ABI_VOID,2,{S,I}},
    {"xl6TimeFormatV1",T,S,4,{M,S,I,I}},
    {"xl6TimeRFC3339V1",T,S,2,{M,I}},
    {"xl6TimeHTTPDateV1",T,S,1,{M}},
    {"xl6TimeParsePatternV1",T,M,4,{S,S,I,I}},
    {"xl6TimeTryParsePatternV1",T,XL6_IR_C_ABI_PRODUCT,4,{S,S,I,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1},
    {"xl6TimeParseAnyV1",T,M,2,{S,I}},
    {"xl6TimeTryParseAnyV1",T,XL6_IR_C_ABI_PRODUCT,2,{S,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1},
    {"xl6TimeParseRFC3339V1",T,M,2,{S,I}},
    {"xl6TimeTryParseRFC3339V1",T,XL6_IR_C_ABI_PRODUCT,2,{S,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1},
    {"xl6TimeParseHTTPDateV1",T,M,2,{S,I}},
    {"xl6TimeTryParseHTTPDateV1",T,XL6_IR_C_ABI_PRODUCT,2,{S,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_BOOLTIME_V1},
    {"xl6DateTimeParseV1",T,V,4,{S,S,I,I},.resultOwner="xl6DateTimeDestroyV1"},
    {"xl6DateTimeFormatV1",T,S,3,{V,S,I},.parameterOwners={"xl6DateTimeDestroyV1"}},
    {"xl6TimeYearV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeMonthV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeDayV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeHourV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeMinuteV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeSecondV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeWeekdayV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeDayOfYearV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeQuarterV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeLeapV1",T,B,1,{I},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeDaysInYearV1",T,I,1,{I},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeDaysInMonthV1",T,I,2,{I,I}},
    {"xl6TimeDatePartV1",T,M,1,{M}},
    {"xl6TimeTimePartV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeSameDayV1",T,B,2,{M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeSameMonthV1",T,B,2,{M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeSameYearV1",T,B,2,{M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeInV1",T,B,3,{M,M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeOverlapV1",T,B,4,{M,M,M,M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeTimerV1",T,F,0,{0}},
    {"xl6TimeSleepV1",T,XL6_IR_C_ABI_VOID,1,{I}},
    {"xl6TimeMonthRangeV1",T,XL6_IR_C_ABI_PRODUCT,1,{M},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_TIMEX2_V1},
    {"xl6TimeYearRangeV1",T,XL6_IR_C_ABI_PRODUCT,1,{M},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_TIMEX2_V1},
    {"xl6TimeWeekRangeV1",T,XL6_IR_C_ABI_PRODUCT,2,{M,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_TIMEX2_V1},
    {"xl6TimeISOWeekV1",T,XL6_IR_C_ABI_PRODUCT,1,{M},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64X3_V1},
    {"xl6DateTimeSplitV1",T,V,2,{M,I},.resultOwner="xl6DateTimeDestroyV1"},
    {"xl6DateTimeFieldsV1",T,V,8,{I,I,I,I,I,I,I,I},.resultOwner="xl6DateTimeDestroyV1"},
    {"xl6DateTimeDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6DateTimeHandlePolicyV1Get"},
    {"xl6DateTimeYearV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeMonthV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeDayV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeHourV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeMinuteV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeSecondV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeMicrosecondV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeOffsetV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeWeekdayV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeDayOfYearV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeIsDSTV1",T,I,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6DateTimeToTimeV1",T,M,1,{V},.parameterOwners={"xl6DateTimeDestroyV1"}},
    {"xl6DateTimeLocalV1",T,V,2,{M,B},.resultOwner="xl6DateTimeDestroyV1"},
    {"xl6TimeFromLocalV1",T,M,2,{V,I},.parameterOwners={"xl6DateTimeDestroyV1"}},
    {"xl6TimeTimezoneOffsetV1",T,I,1,{M}},
    {"xl6TimeFromDateTimeV1",T,M,8,{I,I,I,I,I,I,I,I}},
    {"xl6TimeFromDateV1",T,M,3,{I,I,I}},
    {"xl6TimeFromTimeV1",T,M,4,{I,I,I,I}},
    {"xl6TimeFromUnixV1",T,M,1,{I}},
    {"xl6TimeFromUnixMsV1",T,M,1,{I}},
    {"xl6TimeUnixV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeUnixMsV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeMicrosecondV1",T,I,1,{M},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeNowV1",T,M,0,{0}},
    {"xl6TimeMonotonicV1",T,U,0,{0}},
    {"xl6TimeSleepUsV1",T,XL6_IR_C_ABI_VOID,1,{U}},
    {"xl6TimeSleepUntilV1",T,XL6_IR_C_ABI_VOID,1,{U}},
    {"xl6TimeNearV1",T,B,3,{M,M,U},.noRaise=1,.pure=1,.nonblocking=1},
    {"xl6TimeAddV1",T,M,3,{M,I,I}},
    {"xl6TimeDiffV1",T,I,3,{M,M,I}},
#undef M
#define XL6_NATIVE_COLLECTION_BRIDGE(Name,...) {Name,__VA_ARGS__},
#include "src/abi/xl6_native_collection_contracts.inc"
#undef XL6_NATIVE_COLLECTION_BRIDGE
#define XL6_NATIVE_VALUE_BRIDGE(Name,...) {Name,__VA_ARGS__},
#include "src/abi/xl6_native_value_contracts.inc"
#undef XL6_NATIVE_VALUE_BRIDGE
    {"xl6TextLowerV1",T,S,2,{S,I}},
    {"xl6TextUpperV1",T,S,2,{S,I}},
    {"xl6TextTrimV1",T,S,4,{S,S,I,I}},
    {"xl6TextLTrimV1",T,S,4,{S,S,I,I}},
    {"xl6TextRTrimV1",T,S,4,{S,S,I,I}},
    {"xl6TextFilterV1",T,S,4,{S,S,I,I}},
    {"xl6TextReplaceV1",T,S,6,{S,S,S,I,I,I}},
    {"xl6TextSliceV1",T,S,4,{S,I,I,I}},
    {"xl6TextLeftV1",T,S,3,{S,I,I}},
    {"xl6TextRightV1",T,S,3,{S,I,I}},
    {"xl6TextRepeatV1",T,S,3,{S,I,I}},
    {"xl6TextReverseV1",T,S,2,{S,I}},
    {"xl6TextRemoveAtV1",T,S,4,{S,I,I,I}},
    {"xl6TextInsertV1",T,S,5,{S,I,S,I,I}},
    {"xl6TextRemoveV1",T,S,5,{S,S,B,I,I}},
    {"xl6TextPadLeftV1",T,S,5,{S,I,S,I,I}},
    {"xl6TextPadRightV1",T,S,5,{S,I,S,I,I}},
    {"xl6TextPadCenterV1",T,S,5,{S,I,S,I,I}},
    {"xl6TextLengthV1",T,I,2,{S,I}},
    {"xl6TextByteLengthV1",T,I,2,{S,I}},
    {"xl6TextByteAtV1",T,I,3,{S,I,I},0,XL6_IR_INTEGER_TARGET_UNSIGNED_8},
    {"xl6TextCharAtV1",T,C,3,{S,I,I}},
    {"xl6TextBytesV1",T,Y,2,{S,I}},
    {"xl6TextCompareV1",T,I,5,{S,S,B,I,I}},
    {"xl6TextEqualsV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextContainsV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextIndexOfV1",T,I,5,{S,S,B,I,I}},
    {"xl6TextLastIndexOfV1",T,I,5,{S,S,B,I,I}},
    {"xl6TextCountV1",T,I,5,{S,S,B,I,I}},
    {"xl6TextStartsWithV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextEndsWithV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextHasAnyV1",T,B,4,{S,S,I,I}},
    {"xl6TextIsEmptyV1",T,B,2,{S,I}},
    {"xl6TextIsBlankV1",T,B,2,{S,I}},
    {"xl6TextIsUtf8V1",T,B,2,{S,I}},
    {"xl6TextLikeV1",T,B,5,{S,S,B,I,I}},
    {"xl6TextGlobV1",T,B,5,{S,S,I,I,I}},
    {"xl6TextDistanceV1",T,I,5,{S,S,I,I,I}},
    {"xl6TextSimilarityV1",T,F,4,{S,S,I,I}},
    {"xl6TextApproxV1",T,B,5,{S,S,F,I,I}},
    {"xl6TextIteratorCreateV1",T,V,5,{S,S,I,I,I},.resultOwner="xl6TextIteratorDestroyV1"},
    {"xl6TextIteratorDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6TextIteratorDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6TextIteratorHandlePolicyV1Get"},
    {"xl6TextIteratorNextV1",T,S,1,{V},.parameterOwners={"xl6TextIteratorDestroyV1"},.nullableResult=1},
    {"xl6TextIteratorCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6TextIteratorDestroyV1"}},
    {"xl6TextIteratorClosedV1",T,B,1,{V},.parameterOwners={"xl6TextIteratorDestroyV1"}},
    {"xl6NumberIntFormatV1",T,S,3,{I,S,I}},
    {"xl6NumberUIntFormatV1",T,S,3,{U,S,I}},
    {"xl6NumberFloatFormatV1",T,S,3,{F,S,I}},
    {"xl6NumberBufferIntFormatV1",T,B,4,{V,I,S,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6NumberBufferUIntFormatV1",T,B,4,{V,U,S,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6NumberBufferFloatFormatV1",T,B,4,{V,F,S,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6NumberIntParseV1",T,I,4,{S,I,I,I}},
    {"xl6NumberInt64ParseV1",T,I,4,{S,I,I,I}},
    {"xl6NumberUIntParseV1",T,U,4,{S,I,I,I}},
    {"xl6NumberInt8ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_SIGNED_8},
    {"xl6NumberInt16ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_SIGNED_16},
    {"xl6NumberInt32ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_SIGNED_32},
    {"xl6NumberUInt8ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_8},
    {"xl6NumberUInt16ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_16},
    {"xl6NumberUInt32ParseV1",T,I,4,{S,I,I,I},.resultIntegerTarget=XL6_IR_INTEGER_TARGET_UNSIGNED_32},
    {"xl6NumberFloatParseV1",T,F,3,{S,I,I}},
    {"xl6NumberIntStringV1",T,S,3,{I,I,I}},
    {"xl6NumberUIntStringV1",T,S,3,{U,I,I}},
    {"xl6NumberFloatStringV1",T,S,2,{F,I}},
    {"xl6NumberBufferIntStringV1",T,B,4,{V,I,I,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6NumberBufferUIntStringV1",T,B,4,{V,U,I,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6NumberBufferFloatStringV1",T,B,3,{V,F,I},.parameterOwners={"xl6BufferDestroyV1"}},
    {"xl6UnicodeScalarV1",T,B,1,{I}},
    {"xl6UnicodeCharStringV1",T,S,1,{C}},
    {"xl6UnicodeInvalidTextV1",T,I,2,{S,I}},
    {"xl6UnicodeInvalidBytesV1",T,I,2,{Y,I}},
    {"xl6UnicodeByteOffsetV1",T,I,3,{S,I,I}},
    {"xl6UnicodeScalarIndexV1",T,I,3,{S,I,I}},
    {"xl6UnicodeValidatorCreateV1",T,V,0,{0},.resultOwner="xl6UnicodeValidatorDestroyV1"},
    {"xl6UnicodeValidatorDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6UnicodeValidatorHandlePolicyV1Get"},
    {"xl6UnicodeValidatorFeedV1",T,I,4,{V,Y,B,I},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6UnicodeValidatorResetV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6UnicodeValidatorErrorV1",T,I,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6UnicodeValidatorTotalV1",T,I,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6UnicodeValidatorFinishedV1",T,B,1,{V},.parameterOwners={"xl6UnicodeValidatorDestroyV1"}},
    {"xl6HtmlEscapeTextV1",T,S,2,{S,I}},
    {"xl6HtmlEscapeAttributeV1",T,S,2,{S,I}},
    {"xl6EncodingEncodeV1",T,Y,5,{S,I,B,I,I}},
    {"xl6EncodingDecodeV1",T,S,5,{Y,I,I,I,I}},
    {"xl6EncodingTranscodeV1",T,Y,6,{Y,I,I,I,I,I}},
    {"xl6EncodingBomV1",T,I,2,{Y,I}},
    {"xl6EncodingBomSizeV1",T,I,2,{Y,I}},
    {"xl6EncodingBomBytesV1",T,Y,1,{I}},
    {"xl6EncodingUnitSizeV1",T,I,1,{I}},
    {"xl6EncodingNameV1",T,S,1,{I}},
    {"xl6EncodingRepairTextV1",T,S,2,{S,I}},
    {"xl6EncodingGuessTextV1",T,XL6_IR_C_ABI_PRODUCT,2,{S,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64X3_V1},
    {"xl6EncodingGuessBytesV1",T,XL6_IR_C_ABI_PRODUCT,2,{Y,I},.resultProductAbi=XL6_IR_NATIVE_PRODUCT_I64X3_V1},
    {"xl6RegexCreateV1",T,V,5,{S,I,I,I,I},.resultOwner="xl6RegexDestroyV1"},
    {"xl6RegexDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6RegexHandlePolicyV1Get"},
    {"xl6RegexPatternV1",T,S,1,{V},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexFlagsV1",T,I,1,{V},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexCaptureCountV1",T,I,1,{V},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexCaptureNameV1",T,S,2,{V,I},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexCaptureIndexV1",T,I,3,{V,S,I},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexEscapeV1",T,S,2,{S,I}},
    {"xl6RegexMatchCreateV1",T,V,5,{V,S,I,I,I},.resultOwner="xl6RegexMatchDestroyV1",.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexMatchDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexMatchDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6RegexMatchHandlePolicyV1Get"},
    {"xl6RegexTestV1",T,B,4,{V,S,B,I},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexMatchMatchedV1",T,B,1,{V},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchCountV1",T,I,1,{V},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchTextV1",T,S,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"},.nullableResult=1},
    {"xl6RegexMatchGroupMatchedV1",T,B,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchStartV1",T,I,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchEndV1",T,I,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchNameV1",T,S,2,{V,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchIndexV1",T,I,3,{V,S,I},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexMatchInputV1",T,S,1,{V},.parameterOwners={"xl6RegexMatchDestroyV1"}},
    {"xl6RegexIteratorCreateV1",T,V,5,{V,S,I,I,I},.resultOwner="xl6RegexIteratorDestroyV1",.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexIteratorDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6RegexIteratorHandlePolicyV1Get"},
    {"xl6RegexIteratorHasNextV1",T,B,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexIteratorSnapshotV1",T,V,1,{V},.resultOwner="xl6RegexMatchDestroyV1",.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexIteratorConsumeV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexIteratorCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexIteratorClosedV1",T,B,1,{V},.parameterOwners={"xl6RegexIteratorDestroyV1"}},
    {"xl6RegexReplaceV1",T,S,6,{V,S,S,I,I,I},.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexReplaceBufferV1",T,I,7,{V,V,S,S,I,I,I},.parameterOwners={"xl6RegexDestroyV1","xl6BufferDestroyV1"}},
    {"xl6RegexSplitCreateV1",T,V,5,{V,S,I,I,I},.resultOwner="xl6RegexSplitDestroyV1",.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexSplitDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6RegexSplitHandlePolicyV1Get"},
    {"xl6RegexSplitHasNextV1",T,B,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitSnapshotV1",T,V,1,{V},.resultOwner="xl6RegexSplitPartDestroyV1",.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitConsumeV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitCloseV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitClosedV1",T,B,1,{V},.parameterOwners={"xl6RegexSplitDestroyV1"}},
    {"xl6RegexSplitPartDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6RegexSplitPartHandlePolicyV1Get"},
    {"xl6RegexSplitPartTextV1",T,S,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"},.nullableResult=1},
    {"xl6RegexSplitPartMatchedV1",T,B,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"}},
    {"xl6RegexSplitPartCaptureV1",T,I,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"}},
    {"xl6RegexSplitPartStartV1",T,I,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"}},
    {"xl6RegexSplitPartEndV1",T,I,1,{V},.parameterOwners={"xl6RegexSplitPartDestroyV1"}},
    {"xl6RegexSetDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexSetDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6RegexSetHandlePolicyV1Get"},
    {"xl6RegexSetCountV1",T,I,1,{V},.parameterOwners={"xl6RegexSetDestroyV1"}},
    {"xl6RegexSetPatternV1",T,S,2,{V,I},.parameterOwners={"xl6RegexSetDestroyV1"}},
    {"xl6RegexSetFirstV1",T,I,4,{V,S,I,I},.parameterOwners={"xl6RegexSetDestroyV1"}},
    {"xl6RegexSetTestV1",T,B,4,{V,S,I,I},.parameterOwners={"xl6RegexSetDestroyV1"}},
    {"xl6RegexReplacerCreateV1",T,V,4,{V,S,I,I},.resultOwner="xl6RegexReplacerDestroyV1",.parameterOwners={"xl6RegexDestroyV1"}},
    {"xl6RegexReplacerDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6RegexReplacerDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6RegexReplacerHandlePolicyV1Get"},
    {"xl6RegexReplacerSnapshotV1",T,V,1,{V},.resultOwner="xl6RegexMatchDestroyV1",.parameterOwners={"xl6RegexReplacerDestroyV1"}},
    {"xl6RegexReplacerRunV1",T,S,2,{V,XL6_IR_C_ABI_CLOSURE_PTR},.parameterOwners={"xl6RegexReplacerDestroyV1"},.parameterNullaryCallableResultAbis={0,S}},
    {"xl6RegexReplacerRunBufferV1",T,I,3,{V,V,XL6_IR_C_ABI_CLOSURE_PTR},.parameterOwners={"xl6RegexReplacerDestroyV1","xl6BufferDestroyV1"},.parameterNullaryCallableResultAbis={0,0,S}},
    {"xl6RegexErrorOffsetV1",T,I,1,{XL6_IR_C_ABI_RUNTIME_ERROR_PTR},.noRaise=1},
    {"xl6RegexErrorIndexV1",T,I,1,{XL6_IR_C_ABI_RUNTIME_ERROR_PTR},.noRaise=1},
    {"xl6PatternOptionsCreateV1",T,V,7,{S,I,I,I,I,I,I},.resultOwner="xl6PatternOptionsDestroyV1"},
    {"xl6PatternOptionsDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6PatternOptionsHandlePolicyV1Get"},
    {"xl6PatternOptionsSeparatorsV1",T,S,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsBytesV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsPatternsV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsCapturesV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsStatesV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternOptionsCompiledV1",T,I,1,{V},.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternCreateV1",T,V,2,{S,I},.resultOwner="xl6PatternDestroyV1"},
    {"xl6PatternCreateOptionsV1",T,V,3,{V,S,I},.resultOwner="xl6PatternDestroyV1",.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6PatternHandlePolicyV1Get"},
    {"xl6PatternCountV1",T,I,1,{V},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternCompiledBytesV1",T,I,1,{V},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternSeparatorsV1",T,S,1,{V},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternSourceV1",T,S,2,{V,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternIdV1",T,U,2,{V,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternTagKeyV1",T,I,2,{V,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternCaptureCountV1",T,I,2,{V,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternCaptureNameV1",T,S,3,{V,I,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternCaptureIndexV1",T,I,4,{V,S,I,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternFirstV1",T,I,3,{V,S,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternTestV1",T,B,3,{V,S,I},.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternErrorOffsetV1",T,I,1,{XL6_IR_C_ABI_RUNTIME_ERROR_PTR},.noRaise=1},
    {"xl6PatternErrorIndexV1",T,I,1,{XL6_IR_C_ABI_RUNTIME_ERROR_PTR},.noRaise=1},
    {"xl6PatternMatchCreateV1",T,V,3,{V,S,I},.resultOwner="xl6PatternMatchDestroyV1",.parameterOwners={"xl6PatternDestroyV1"}},
    {"xl6PatternMatchDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6PatternMatchHandlePolicyV1Get"},
    {"xl6PatternMatchMatchedV1",T,B,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchCountV1",T,I,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchPatternIndexV1",T,I,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchIdV1",T,U,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchInputV1",T,S,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchSourceV1",T,S,1,{V},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchTextV1",T,S,2,{V,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchStartV1",T,I,2,{V,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchEndV1",T,I,2,{V,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchNameV1",T,S,2,{V,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternMatchIndexV1",T,I,3,{V,S,I},.parameterOwners={"xl6PatternMatchDestroyV1"}},
    {"xl6PatternBuilderCreateV1",T,V,0,{0},.resultOwner="xl6PatternBuilderDestroyV1"},
    {"xl6PatternBuilderOptionsV1",T,V,1,{V},.resultOwner="xl6PatternBuilderDestroyV1",.parameterOwners={"xl6PatternOptionsDestroyV1"}},
    {"xl6PatternBuilderDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternBuilderDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6PatternBuilderHandlePolicyV1Get"},
    {"xl6PatternBuilderCountV1",T,I,1,{V},.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternBuilderVersionV1",T,U,1,{V},.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternBuilderIdentityV1",T,V,1,{V},.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternStateCreateV1",T,V,1,{V},.parameterDynamicBorrows={1}},
    {"xl6PatternBuilderReserveV1",T,XL6_IR_C_ABI_VOID,2,{V,I},.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternBuilderContainsV1",T,B,2,{V,V},.parameterOwners={"xl6PatternBuilderDestroyV1","xl6PatternKeyDestroyV1"}},
    {"xl6PatternBuilderCompileV1",T,V,1,{V},.resultOwner="xl6PatternDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternEditAddV1",T,V,4,{V,S,I,I},.resultOwner="xl6PatternEditDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternEditSetV1",T,V,5,{V,V,S,I,I},.resultOwner="xl6PatternEditDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1","xl6PatternKeyDestroyV1"}},
    {"xl6PatternEditRemoveV1",T,V,2,{V,V},.resultOwner="xl6PatternEditDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1","xl6PatternKeyDestroyV1"}},
    {"xl6PatternEditClearV1",T,V,1,{V},.resultOwner="xl6PatternEditDestroyV1",.parameterOwners={"xl6PatternBuilderDestroyV1"}},
    {"xl6PatternEditDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternEditDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6PatternEditHandlePolicyV1Get"},
    {"xl6PatternEditCountV1",T,I,1,{V},.parameterOwners={"xl6PatternEditDestroyV1"}},
    {"xl6PatternEditCancelV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternEditDestroyV1"}},
    {"xl6PatternEditApplyV1",T,XL6_IR_C_ABI_VOID,3,{V,V,V},.parameterOwners={"xl6PatternEditDestroyV1"},.parameterDynamicBorrows={0,1,1}},
    {"xl6PatternKeyCreateV1",T,V,2,{V,I},.resultOwner="xl6PatternKeyDestroyV1",.parameterOwners={"xl6PatternEditDestroyV1"}},
    {"xl6PatternKeyDestroyV1",T,XL6_IR_C_ABI_VOID,1,{V},.parameterOwners={"xl6PatternKeyDestroyV1"},.noRaise=1,.nativeHandlePolicy="xl6PatternKeyHandlePolicyV1Get"},
    {"xl6PatternKeyIdV1",T,U,1,{V},.parameterOwners={"xl6PatternKeyDestroyV1"}},
#undef U
#undef F
#undef Y
#undef V
#undef C
#undef B
#undef S
#undef I
#undef T
    { "xl6_xrt_write", XL6_IR_RUNTIME_BRIDGE_TEXT_WRITE, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING }, .noRaise=1 },
    { "xl6_xrt_writeln", XL6_IR_RUNTIME_BRIDGE_TEXT_WRITELN, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING }, .noRaise=1 },
    { "xl6_xrt_console_error", XL6_IR_RUNTIME_BRIDGE_TEXT_ERROR, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING }, .noRaise=1 },
    { "xl6_xrt_console_error_line", XL6_IR_RUNTIME_BRIDGE_TEXT_ERRORLN, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING }, .noRaise=1 },
    { "xl6_xrt_console_flush", XL6_IR_RUNTIME_BRIDGE_TEXT_FLUSH, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_C_INT }, .noRaise=1 },
    { "xl6_xrt_dir_list", XL6_IR_RUNTIME_BRIDGE_DIR_LIST, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_INT }, .noRaise=1 },
    { "xl6_xrt_dir_glob", XL6_IR_RUNTIME_BRIDGE_DIR_GLOB, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
      4, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_INT }, .noRaise=1 },
    { "xl6_xrt_dir_entries", XL6_IR_RUNTIME_BRIDGE_DIR_ENTRIES, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
      5, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_INT }, .noRaise=1 },
    { "xl6_xrt_dir_is_empty", XL6_IR_RUNTIME_BRIDGE_DIR_IS_EMPTY, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING }, .noRaise=1 },
    { "xl6_xrt_dir_count", XL6_IR_RUNTIME_BRIDGE_DIR_COUNT, XL6_IR_C_ABI_I64,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_INT }, .noRaise=1 },
    { "xl6_xrt_dir_size", XL6_IR_RUNTIME_BRIDGE_DIR_SIZE, XL6_IR_C_ABI_I64,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL }, .noRaise=1 },
    { "xl6_xrt_hash32_string", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_UINT,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash32_string_seed", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_UINT,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_UINT, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash64_string", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_ULONGLONG,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash64_string_seed", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_ULONGLONG, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash32_bytes", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_UINT,
      2, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash32_bytes_seed", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_UINT,
      3, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_C_UINT, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash64_bytes", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_ULONGLONG,
      2, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash64_bytes_seed", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_C_ULONGLONG, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash32_raw", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_UINT,
      3, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash32_raw_seed", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_UINT,
      4, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_C_UINT, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash64_raw", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_hash64_raw_seed", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_ULONGLONG,
      4, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_C_ULONGLONG, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_siphash64_string", XL6_IR_RUNTIME_BRIDGE_SIPHASH_STRING, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_siphash64_bytes", XL6_IR_RUNTIME_BRIDGE_SIPHASH_BYTES, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 }, .noRaise=1 },
    { "xl6_xrt_siphash64_raw", XL6_IR_RUNTIME_BRIDGE_SIPHASH_RAW, XL6_IR_C_ABI_C_ULONGLONG,
      4, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 }, .noRaise=1 }
};

static const XL6_IrDirectBridgeContract* xl6IrDirectBridgeContract(const char* name)
{
    size_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < sizeof(xl6IrDirectBridgeContracts) / sizeof(xl6IrDirectBridgeContracts[0]); ++i)
        if (strcmp(name, xl6IrDirectBridgeContracts[i].name) == 0)
            return &xl6IrDirectBridgeContracts[i];
    return NULL;
}

/* Typed declarations retain a quoted linkage token; Lowered targets already
 * carry the canonical identifier. Both select the same exact registry row. */
static const XL6_IrDirectBridgeContract* xl6IrDirectBridgeContractSpelling(const char* name)
{
    const XL6_IrDirectBridgeContract* result=xl6IrDirectBridgeContract(name);
    if (result || !name) return result;
    size_t length=strlen(name);
    if (length>=2 && name[0]=='"' && name[length-1]=='"')
        for(size_t i=0;i<sizeof(xl6IrDirectBridgeContracts)/sizeof(xl6IrDirectBridgeContracts[0]);++i) {
            const XL6_IrDirectBridgeContract* row=&xl6IrDirectBridgeContracts[i];
            if(strlen(row->name)==length-2 && !memcmp(name+1,row->name,length-2))return row;
        }
    return NULL;
}
static int xl6IrInternNativeCallValueSchemas(XL6_IrModule* module)
{
    if (!module) return 0;
    for(size_t n=0;n<module->nodeCount;++n) {
        const XL6_IrNode* method=&module->nodes[n];
        if(method->kind!=XL6_IR_NODE_FUNC_DECL)continue;
        const XL6_IrDirectBridgeContract* row=xl6IrDirectBridgeContractSpelling(method->text);
        if(!row || !row->sequenceValueResult)continue;
        XL6_IrNodeId owner=method->parent;
        while(owner) {
            const XL6_IrNode* node=xl6IrNodeGet(module,owner);
            if(!node)return 0;
            if(node->kind==XL6_IR_NODE_TYPE_DECL && node->sourceKind==XL6_AST_CLASS)break;
            owner=node->parent;
        }
        if(!owner)continue; /* invalid free-function signatures are rejected separately */
        for(size_t i=0;i<module->types.count;++i) {
            const XL6_IrType* type=&module->types.items[i];
            if(type->kind!=XL6_IR_TYPE_CLASS || type->paramCount!=1 || !type->params ||
                !xl6IrBuildClassTypeIsConcreteInstance(module,owner,(XL6_IrTypeId)(i+1)))continue;
            XL6_IrTypeId element=type->params[0];
            if(!xl6IrTypeInternArray(&module->types,element))return 0;
        }
    }
    return 1;
}

XL6_IrRuntimeBridgeKind xl6IrRuntimeDirectBridgeKind(const char* name)
{
    const XL6_IrDirectBridgeContract* contract = xl6IrDirectBridgeContract(name);
    return contract != NULL ? contract->kind : XL6_IR_RUNTIME_BRIDGE_NONE;
}

XL6_IrCallNativeArrayResultAbi xl6IrRuntimeDirectBridgeNativeArrayResultAbi(const char* name)
{
    const XL6_IrDirectBridgeContract* contract = xl6IrDirectBridgeContract(name);
    return contract ? contract->nativeArrayResultAbi : XL6_IR_CALL_NATIVE_ARRAY_RESULT_NONE;
}

const char* xl6IrRuntimeNativeHandlePolicyForName(const char* name)
{
    const XL6_IrDirectBridgeContract* contract = xl6IrDirectBridgeContract(name);
    if (contract != NULL) return contract->nativeHandlePolicy;
    /* Typed declarations retain the quoted C linkage literal; lowered targets
     * store its canonical identifier. Match the SAME exact registry entry
     * without allocating, interpreting escapes or recognizing name prefixes. */
    if (name != NULL) {
        size_t length = strlen(name);
        if (length >= 2 && name[0] == '"' && name[length-1] == '"')
            for (size_t i=0;i<sizeof(xl6IrDirectBridgeContracts)/sizeof(xl6IrDirectBridgeContracts[0]);++i) {
                contract=&xl6IrDirectBridgeContracts[i];
                if (contract->nativeHandlePolicy != NULL && strlen(contract->name) == length-2 &&
                    memcmp(name+1,contract->name,length-2) == 0) return contract->nativeHandlePolicy;
            }
    }
    /* Dedicated bridge families use the same exact resource declaration as
     * direct calls. This is linkage metadata, not Cgen symbol heuristics. */
    if(name){
        size_t length=strlen(name);const char* canonical=name;
        if(length>=2 && name[0]=='"' && name[length-1]=='"'){canonical=name+1;length-=2;}
        static const struct {const char* owner;const char* getter;} resources[]={
#define XL6_NATIVE_RESOURCE_POLICY(Owner,Component,Getter) {Owner,#Getter},
#include "src/runtime/xl6_native_resource_policies.inc"
#undef XL6_NATIVE_RESOURCE_POLICY
        };
        for(size_t i=0;i<sizeof(resources)/sizeof(resources[0]);++i)
            if(resources[i].owner && strlen(resources[i].owner)==length && !memcmp(canonical,resources[i].owner,length))return resources[i].getter;
    }
    return NULL;
}

int xl6IrRuntimeDirectBridgeFactory(const char* name)
{ const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name); return c && c->resultOwner; }
XL6_IrErrorAbi xl6IrRuntimeDirectBridgeErrorAbi(const char* name)
{
    const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name);
    return c && !c->noRaise
        ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;
}
XL6_IrCallErrorPhysicalAbi xl6IrRuntimeDirectBridgeErrorPhysicalAbi(const char* name)
{
    const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name);
    if (!c) return XL6_IR_CALL_ERROR_PHYSICAL_INVALID;
    if (c->noRaise) return XL6_IR_CALL_ERROR_PHYSICAL_NONE;
    if (c->errorPhysicalAbi) return c->errorPhysicalAbi;
    return c->nativeArrayResultAbi ? XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT
        : XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR;
}
const char* xl6IrRuntimeDirectBridgeParameterOwner(const char* name, size_t index)
{
    const XL6_IrDirectBridgeContract* c = xl6IrDirectBridgeContract(name);
    return c && index < c->parameterCount ? c->parameterOwners[index] : NULL;
}
int xl6IrRuntimeDirectBridgeCheckedClosureParameter(const char* name,size_t index)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c && index<c->parameterCount &&
        (c->parameterNullaryCallableResultAbis[index]!=0 || c->genericFutureStarterParameter==index+1);
}
int xl6IrRuntimeDirectBridgeResultClosureParameter(const char* name,size_t index)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c&&index<c->parameterCount&&c->genericFutureValueCallbackParameter==index+1;
}
int xl6IrRuntimeDirectBridgeValueAddressParameter(const char* name,size_t index)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContractSpelling(name);
    return c && c->genericValueParameter==index+1;
}
XL6_IrCallNativeResultAbi xl6IrRuntimeDirectBridgeNativeResultAbi(const char* name)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c ? c->nativeResultAbi : XL6_IR_CALL_NATIVE_RESULT_NONE;
}
int xl6IrRuntimeDirectBridgeManagedHandleResult(const char* name)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c && c->resultOwner && c->managedHandleResult;
}
XL6_IrTypeId xl6IrNativeContextElementType(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if (!module || !target) return XL6_IR_TYPE_INVALID;
    if (target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1)
        return target->nativeContextType;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,target->nativeContextType);
    return owner && owner->params && owner->paramCount==1 ? owner->params[0] : XL6_IR_TYPE_INVALID;
}
const XL6_IrClassField* xl6IrNativeClassArrayContextField(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* c=target ? xl6IrDirectBridgeContract(target->cName) : NULL;
    if(!module || !c || !c->nativeClassArrayOwner || !c->nativeClassArrayParameter ||
        c->nativeClassArrayParameter>target->parameterCount || !target->parameterTypes)return NULL;
    XL6_IrTypeId id=target->parameterTypes[c->nativeClassArrayParameter-1];
    const XL6_IrType* array=xl6IrTypeGet(&module->types,id);
    if(!array || array->collectionAbi!=XL6_IR_COLLECTION_ARRAY || array->paramCount!=1 || !array->params ||
        !xl6IrSequenceConstructionRecipe(module,id,XL6_IR_SEQUENCE_ROLE_ARRAY,NULL))return NULL;
    return xl6IrResidentNativeResourceField(module,array->params[0],c->nativeClassArrayOwner);
}
const XL6_IrClassField* xl6IrNativeClassContextField(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* c=target?xl6IrDirectBridgeContract(target->cName):NULL;
    if(!module || !c || !c->nativeClassOwner || !c->nativeClassParameter ||
        c->nativeClassParameter>target->parameterCount || !target->parameterTypes)return NULL;
    return xl6IrResidentNativeResourceField(module,
        target->parameterTypes[c->nativeClassParameter-1],c->nativeClassOwner);
}
unsigned xl6IrRuntimeDirectBridgeServices(const char* name)
{
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(name);
    return c ? c->nativeServicesRequired : XL6_IR_CALL_NATIVE_SERVICE_NONE;
}

static const XL6_IrType* xl6IrNativeVoidCallbackArrayType(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrDirectBridgeContract* contract)
{
    if(!module||!target||!contract||!contract->nativeVoidCallbackArrayParameter||!target->parameterTypes||
        contract->nativeVoidCallbackArrayParameter>target->parameterCount)return NULL;
    XL6_IrTypeId id=target->parameterTypes[contract->nativeVoidCallbackArrayParameter-1];
    const XL6_IrType* array=xl6IrTypeGet(&module->types,id);
    if(!array||array->kind!=XL6_IR_TYPE_GENERIC_INSTANCE||array->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR||
        array->collectionAbi!=XL6_IR_COLLECTION_ARRAY||array->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||
        array->nullAbi!=XL6_IR_NULL_ABI_XVALUE_SENTINEL||array->paramCount!=1||!array->params||
        array->returnCount||array->returns||!array->typeValueId)return NULL;
    const XL6_IrType* callback=xl6IrTypeGet(&module->types,array->params[0]);
    if(!callback||callback->kind!=XL6_IR_TYPE_FUNCTION||callback->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR||
        callback->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||callback->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL||
        callback->collectionAbi!=XL6_IR_COLLECTION_NONE||callback->paramCount||callback->params||
        callback->parameterAbis||callback->parameterModes||callback->returnCount||callback->returns||
        callback->callableResultType||callback->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED||!callback->typeValueId)return NULL;
    XL6_IrSequenceLayout expected=xl6IrSequenceLayoutForType(&module->types,array);
    if(expected.abi!=XL6_IR_SEQUENCE_LAYOUT_CALLABLE_REFERENCE_V1||expected.nullable||
        expected.elementType!=array->params[0]||expected.slotAbi!=XL6_IR_NATIVE_SLOT_XVALUE_CARRIER||
        expected.codecAbi!=XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1||
        !xl6IrSequenceLayoutEqual(&array->sequenceLayout,&expected)||
        !xl6IrSequenceConstructionRecipe(module,id,XL6_IR_SEQUENCE_ROLE_ARRAY,NULL))return NULL;
    return array;
}
int xl6IrNativeCallValuePlanBuild(const XL6_IrModule* module,const XL6_IrCallTarget* target,
    XL6_IrNativeCallValuePlan* output)
{
    if (!module || !target) return 0;
    XL6_IrNativeCallValuePlan plan={0};
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(target->cName);
    if (c && (c->genericChannelParameter || c->sequenceValueResult)) {
        if (!target->parameterTypes || target->parameterCount!=c->parameterCount ||
            c->errorPhysicalAbi!=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE) return 0;
        if (c->genericChannelParameter) {
            if (c->genericChannelParameter>target->parameterCount ||
                !xl6IrChannelValuePlanBuild(module,target->parameterTypes[c->genericChannelParameter-1],&plan.value)) return 0;
        } else {
            const XL6_IrType* owner=xl6IrTypeGet(&module->types,target->parameterTypes[0]);
            if (!owner || owner->kind!=XL6_IR_TYPE_CLASS || owner->paramCount!=1 || !owner->params ||
                !xl6IrChannelValuePlanFromSchema(module,
                    xl6IrArraySchemaTypeForElement(&module->types,owner->params[0]),&plan.value)) return 0;
        }
        plan.inputParameter=c->genericValueParameter;
        plan.output=c->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1;
        if ((plan.inputParameter && (plan.inputParameter>target->parameterCount ||
             target->parameterTypes[plan.inputParameter-1]!=plan.value.elementType)) ||
            (plan.output && target->returnType!=plan.value.elementType)) return 0;
    }
    if (output) *output=plan;
    return 1;
}

int xl6IrRuntimeDirectBridgeContext(const XL6_IrModule* module,const XL6_IrCallTarget* target,
    XL6_IrCallNativeContextAbi* context,XL6_IrTypeId* type,XL6_IrAsyncResultAbi* payload)
{
    if(!module || !target || !context || !type || !payload)return 0;
    *context=XL6_IR_CALL_NATIVE_CONTEXT_NONE;*type=XL6_IR_TYPE_INVALID;*payload=XL6_IR_ASYNC_RESULT_INVALID;
    const XL6_IrDirectBridgeContract* c=xl6IrDirectBridgeContract(target->cName);
    if(!c || !c->nativeContext)return 1;
    if(c->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_NULLARY_VOID_ARRAY_V1){
        if(!xl6IrNativeVoidCallbackArrayType(module,target,c))return 0;
        *context=c->nativeContext;*type=target->parameterTypes[c->nativeVoidCallbackArrayParameter-1];return 1;
    }
    if(c->genericFutureStarterParameter||c->genericFutureValueCallbackParameter) {
        const XL6_IrType* future=xl6IrTypeGet(&module->types,target->returnType);
        if(c->nativeContext!=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1 || !c->genericFutureResult ||
           !future || future->kind!=XL6_IR_TYPE_GENERIC_INSTANCE ||
           future->cAbi!=XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
           future->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
           future->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
           future->collectionAbi!=XL6_IR_COLLECTION_NONE || future->returnCount ||
           future->paramCount!=1 || !future->params)return 0;
        const XL6_IrType* value=xl6IrTypeGet(&module->types,future->params[0]);
        XL6_IrAsyncResultAbi abi=xl6IrBuildAsyncResultAbi(module,future->params[0]);
        if(!value || !value->typeValueId || abi==XL6_IR_ASYNC_RESULT_INVALID)return 0;
        *context=c->nativeContext;*type=future->params[0];*payload=abi;return 1;
    }
    if(c->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1) {
        if(!xl6IrNativeClassContextField(module,target))return 0;
        *context=c->nativeContext;*type=target->parameterTypes[c->nativeClassParameter-1];
        *payload=xl6IrBuildAsyncResultAbi(module,*type);
        return *payload==XL6_IR_ASYNC_RESULT_RETAINED_XVALUE;
    }
    if(c->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1) {
        if(!xl6IrNativeClassArrayContextField(module,target))return 0;
        *context=c->nativeContext;*type=target->parameterTypes[c->nativeClassArrayParameter-1];return 1;
    }
    if(c->nativeContext==XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1) {
        if(!c->genericChannelParameter || c->genericChannelParameter>target->parameterCount || !target->parameterTypes)
            return 0;
        XL6_IrTypeId owner=target->parameterTypes[c->genericChannelParameter-1];
        XL6_IrNativeCallValuePlan message={0};
        if(!xl6IrNativeCallValuePlanBuild(module,target,&message))return 0;
        *context=c->nativeContext;*type=owner;return 1;
    }
    XL6_IrTypeId ownerId=c->resultOwner?target->returnType:
        target->parameterCount && target->parameterTypes?target->parameterTypes[0]:XL6_IR_TYPE_INVALID;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,ownerId);
    if(!owner || owner->kind!=XL6_IR_TYPE_CLASS || owner->paramCount!=1 || !owner->params)return 0;
    const XL6_IrType* value=xl6IrTypeGet(&module->types,owner->params[0]);
    if(!value || !value->typeValueId)return 0;
    XL6_IrAsyncResultAbi abi=xl6IrBuildAsyncResultAbi(module,owner->params[0]);
    if (c->sequenceValueResult) {
        XL6_IrNativeCallValuePlan message={0};
        if (!xl6IrNativeCallValuePlanBuild(module,target,&message)) return 0;
        if (message.value.codecAbi!=XL6_IR_SEQUENCE_CODEC_DIRECT)
            abi=XL6_IR_ASYNC_RESULT_RETAINED_XVALUE;
    }
    if(abi==XL6_IR_ASYNC_RESULT_INVALID)return 0;
    *context=c->nativeContext;*type=owner->params[0];*payload=abi;return 1;
}
static int xl6IrDirectBridgeResourceMatches(const XL6_IrModule* module,
    XL6_IrTypeId id, XL6_IrNodeId fieldId, XL6_SymbolId destroyId, const char* owner)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, id);
    if (!type || type->kind != XL6_IR_TYPE_CLASS ||
        type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL || fieldId == XL6_IR_NODE_INVALID) return 0;
    if (module->stage == XL6_IR_STAGE_LOWERED) {
        /* The physical contract selects ONE native field, not a one-field
         * language class. Other managed fields retain their own ordinary
         * layout/trace/destruction contracts; the payload's leaf policy does
         * not certify or hide them. No name/order/first-field inference. */
        const XL6_IrClassField* field = xl6IrManagedNativeConstructorField(module, id);
        if (!field) return 0;
        const XL6_IrCallTarget* destroy = xl6IrCallTargetGet(module, field->nativeDestroySymbol);
        if (field->declaration != fieldId || !destroy || !destroy->cName ||
            strcmp(destroy->cName, owner) || (destroyId && destroyId != field->nativeDestroySymbol)) return 0;
    }
    return 1;
}

static int xl6IrDirectBridgeResultResourceMatches(const XL6_IrModule* module,
    const XL6_IrCallTarget* target, const XL6_IrDirectBridgeContract* contract)
{
    XL6_IrTypeId owner = target->returnType;
    const XL6_IrType* type = xl6IrTypeGet(&module->types, owner);
    if (!type || (type->kind == XL6_IR_TYPE_NULLABLE) != !!contract->nullableResult) return 0;
    if (contract->nullableResult) {
        /* NULL is an ordinary success value, never an error discriminator.
         * A separate checked outcome is mandatory for optional owners. */
        if (contract->managedHandleResult || type->paramCount != 1 || !type->params ||
            type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
            type->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL ||
            type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
            (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
             target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE)) return 0;
        owner = type->params[0];
    }
    return xl6IrDirectBridgeResourceMatches(module, owner,
        target->resultPhysicalFieldDeclaration, target->resultPhysicalDestroySymbol, contract->resultOwner);
}

static int xl6IrDirectBridgeCollectionMatches(const XL6_IrModule* module,
    const XL6_IrType* type, XL6_IrCAbi itemAbi, XL6_IrCollectionAbi collection)
{
    const XL6_IrType* item = type && type->paramCount == 1 && type->params
        ? xl6IrTypeGet(&module->types, type->params[0]) : NULL;
    if (!type || type->kind != XL6_IR_TYPE_GENERIC_INSTANCE || type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->collectionAbi != collection || type->returnCount != 0 ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL || !item ||
        item->kind != XL6_IR_TYPE_BUILTIN || item->paramCount != 0 ||
        item->collectionAbi != XL6_IR_COLLECTION_NONE || item->returnCount != 0 ||
        item->cAbi != itemAbi) return 0;
    if (itemAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR)
        return xl6IrSerializationBridgeDynamicValueType(item);
    if (itemAbi == XL6_IR_C_ABI_MANAGED_STRING)
        return item->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            item->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL;
    if (itemAbi == XL6_IR_C_ABI_MANAGED_BYTES)
        return item->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            item->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL;
    if (itemAbi == XL6_IR_C_ABI_I64 && item->integerTarget != XL6_IR_INTEGER_TARGET_SIGNED_64) return 0;
    if (itemAbi == XL6_IR_C_ABI_U64 && item->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_64) return 0;
    return xl6IrBufferRuntimeBridgeResultMatches(item, itemAbi);
}

int xl6IrRuntimeDirectBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* contract;
    const XL6_IrType* result;
    size_t i;
    if (module == NULL || target == NULL ||
        (contract = xl6IrDirectBridgeContract(target->cName)) == NULL ||
        target->kind != (contract->resultOwner ? XL6_IR_CALL_TARGET_NATIVE_FACTORY : XL6_IR_CALL_TARGET_EXTERN_C) ||
        target->runtimeBridge != contract->kind || target->errorAbi !=
            xl6IrRuntimeDirectBridgeErrorAbi(contract->name) ||
        target->nativeArrayResultAbi != contract->nativeArrayResultAbi ||
        target->nativeResultAbi != contract->nativeResultAbi ||
        target->nativeServicesRequired != contract->nativeServicesRequired ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
         target->errorPhysicalAbi != xl6IrRuntimeDirectBridgeErrorPhysicalAbi(contract->name)) ||
        (contract->resultOwner
            ? (target->resultPhysicalAbi != (contract->managedHandleResult
                    ? XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_HANDLE_VALUE_TAKE
                    : XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE) ||
               !target->resultPhysicalDestroySymbol ||
               !xl6IrDirectBridgeResultResourceMatches(module, target, contract))
            : (target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
               target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID || target->resultPhysicalDestroySymbol != 0)) ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
        target->parameterCount != contract->parameterCount ||
        (target->parameterCount && (target->parameterTypes == NULL || target->parameterAbis == NULL ||
         target->parameterPhysicalAbis == NULL || target->parameterConstraintAbis == NULL ||
         target->parameterPhysicalFieldDeclarations == NULL))) return 0;
    result = xl6IrTypeGet(&module->types, target->returnType);
    {
        XL6_IrCallNativeContextAbi context;XL6_IrTypeId type;XL6_IrAsyncResultAbi payload;
        if(!xl6IrRuntimeDirectBridgeContext(module,target,&context,&type,&payload) ||
           context!=target->nativeContextAbi || type!=target->nativeContextType ||
           payload!=target->nativeContextPayloadAbi)return 0;
    }
    if(contract->genericResourceResult || contract->genericResourceParameter) {
        XL6_IrTypeId expected=target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_NONE
            ? result && result->kind==XL6_IR_TYPE_CLASS && result->paramCount==1 && result->params
                ? result->params[0] : XL6_IR_TYPE_INVALID
            : xl6IrNativeContextElementType(module,target);
        if(!expected && target->nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_NONE &&
            contract->genericResourceParameter &&
            contract->genericResourceParameter<=target->parameterCount) {
            const XL6_IrType* resource=xl6IrTypeGet(&module->types,
                target->parameterTypes[contract->genericResourceParameter-1]);
            if(resource && resource->kind==XL6_IR_TYPE_CLASS && resource->paramCount==1 && resource->params)
                expected=resource->params[0];
        }
        if(contract->genericResourceResult && (!expected || !result || result->kind!=XL6_IR_TYPE_CLASS ||
            result->paramCount!=1 || !result->params || result->params[0]!=expected))return 0;
        if(contract->genericResourceParameter) {
            size_t index=contract->genericResourceParameter-1;
            const XL6_IrType* resource=index<target->parameterCount
                ? xl6IrTypeGet(&module->types,target->parameterTypes[index]):NULL;
            if(!resource || resource->kind!=XL6_IR_TYPE_CLASS || resource->paramCount!=1 || !resource->params ||
                !expected || resource->params[0]!=expected)return 0;
        }
    }
    if (contract->nativeArrayResultAbi) {
        if (!result || !result->typeValueId || result->paramCount != 1 || !result->params ||
            !xl6IrDirectBridgeCollectionMatches(module, result, contract->resultArrayItemAbi, XL6_IR_COLLECTION_ARRAY)) return 0;
        const XL6_IrType* item = xl6IrTypeGet(&module->types, result->params[0]);
        XL6_IrSequenceLayout expected = xl6IrSequenceLayoutForType(&module->types, result);
        if (!item || !item->typeValueId || expected.abi != XL6_IR_SEQUENCE_LAYOUT_PRIMITIVE_V2 ||
            expected.nullable || expected.elementType != result->params[0] ||
            !xl6IrSequenceLayoutEqual(&result->sequenceLayout, &expected)) return 0;
    }
    if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE ||
        contract->kind == XL6_IR_RUNTIME_BRIDGE_PRINT) {
        const XL6_IrNode* declaration = xl6IrNodeGet(module, target->declaration);
        if (target->effectAbi != (contract->pure ? XL6_IR_CALL_EFFECT_PURE : XL6_IR_CALL_EFFECT_CONSERVATIVE) ||
            target->executionBoundaryAbi != (contract->nonblocking
                ? XL6_IR_CALL_EXECUTION_FOREIGN_NONBLOCKING : XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK) ||
            !declaration || (!contract->noRaise && (declaration->flags & XL6_AST_FLAG_FOREIGN_NO_RAISE))) return 0;
        if (contract->result == XL6_IR_C_ABI_I64 && (!result ||
            result->integerTarget != (contract->resultIntegerTarget ? contract->resultIntegerTarget
                : XL6_IR_INTEGER_TARGET_SIGNED_64))) return 0;
        if (contract->result == XL6_IR_C_ABI_U64 && (!result ||
            result->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_64)) return 0;
    }
    if (contract->nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1) {
        if(!result || target->returnType!=xl6IrNativeContextElementType(module,target) ||
            contract->resultOwner || contract->noRaise || contract->result ||
            target->errorPhysicalAbi==XL6_IR_CALL_ERROR_PHYSICAL_NONE)return 0;
    } else if (contract->result == XL6_IR_C_ABI_VOID) {
        if (target->returnType != XL6_IR_TYPE_INVALID) return 0;
    } else if (!contract->resultOwner) {
        if(contract->resultEnum) {
            if(contract->nullableResult||contract->resultProductAbi||contract->resultArrayItemAbi||
                contract->resultFutureElementAbi||contract->resultFutureAbi||
                contract->resultNativeValueAbi||contract->resultEnum->cAbi!=contract->result||
                !xl6IrDirectNumericEnumMatches(module,target->returnType,contract->resultEnum)||
                !xl6IrBufferRuntimeBridgeResultMatches(result,contract->result))return 0;
        } else if (contract->resultNativeValueAbi) {
            if (contract->result != XL6_IR_C_ABI_NATIVE_VALUE ||
                !xl6IrNativeValuePlanForType(result) ||
                result->nativeValueAbi != contract->resultNativeValueAbi ||
                contract->nullableResult || contract->resultProductAbi ||
                contract->resultArrayItemAbi || contract->resultFutureElementAbi || contract->resultFutureAbi) return 0;
        } else if (contract->resultProductAbi) {
            if (!result || result->cAbi != XL6_IR_C_ABI_PRODUCT ||
                result->nativeProductAbi != contract->resultProductAbi ||
                xl6IrNativeProductAbiForShape(&module->types,result) != contract->resultProductAbi ||
                result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL || result->nullAbi != XL6_IR_NULL_ABI_NONE ||
                strcmp(result->cName,xl6IrNativeProductAbiCName(contract->resultProductAbi)) != 0) return 0;
            for(size_t p=0;p<3;++p){
                const XL6_IrNumericEnumContract* expected=contract->resultProductEnums[p];
                if(expected){
                    if(p>=result->paramCount||!xl6IrDirectNumericEnumMatches(module,result->params[p],expected))return 0;
                }else if(p<result->paramCount){
                    const XL6_IrType* item=xl6IrTypeGet(&module->types,result->params[p]);
                    if(!item||item->kind!=XL6_IR_TYPE_BUILTIN)return 0;
                }
            }
        } else if(contract->genericFutureResult) {
            if(!result || result->kind!=XL6_IR_TYPE_GENERIC_INSTANCE ||
               result->cAbi!=XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
               result->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
               result->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
               result->collectionAbi!=XL6_IR_COLLECTION_NONE || result->returnCount ||
               result->paramCount!=1 || !result->params || result->params[0]!=target->nativeContextType)return 0;
        } else if (contract->resultFutureElementAbi || contract->resultFutureAbi) {
            const XL6_IrType* element = result && result->paramCount == 1 && result->params
                ? xl6IrTypeGet(&module->types, result->params[0]) : NULL;
            if (contract->result != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR || !result ||
                result->kind != XL6_IR_TYPE_GENERIC_INSTANCE ||
                result->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
                result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
                result->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL ||
                result->collectionAbi != XL6_IR_COLLECTION_NONE || result->returnCount ||
                !element || element->kind != XL6_IR_TYPE_BUILTIN || element->paramCount || element->returnCount ||
                element->collectionAbi != XL6_IR_COLLECTION_NONE) return 0;
            if(contract->resultFutureAbi){
                /* A managed codec alone may describe several builtins. An
                 * explicit element C ABI additionally seals its logical
                 * family (for example bytes, not arbitrary xvalue). */
                if((contract->resultFutureElementAbi&&element->cAbi!=contract->resultFutureElementAbi) ||
                    xl6IrBuildAsyncResultAbi(module,result->params[0])!=contract->resultFutureAbi)return 0;
            }else if(element->cAbi!=contract->resultFutureElementAbi ||
                element->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
                element->nullAbi!=XL6_IR_NULL_ABI_NONE ||
                (contract->resultFutureElementAbi==XL6_IR_C_ABI_I64 && element->integerTarget!=
                    (contract->resultIntegerTarget?contract->resultIntegerTarget:XL6_IR_INTEGER_TARGET_SIGNED_64)) ||
                (contract->resultFutureElementAbi==XL6_IR_C_ABI_U64 && element->integerTarget!=
                    XL6_IR_INTEGER_TARGET_UNSIGNED_64))return 0;
        } else if (contract->resultArrayItemAbi) {
            if (!xl6IrDirectBridgeCollectionMatches(module, result, contract->resultArrayItemAbi,
                contract->resultCollectionAbi ? contract->resultCollectionAbi : XL6_IR_COLLECTION_ARRAY)) return 0;
        } else {
            if (result == NULL || result->kind != (contract->nullableResult ? XL6_IR_TYPE_NULLABLE : XL6_IR_TYPE_BUILTIN) ||
                result->collectionAbi != XL6_IR_COLLECTION_NONE || result->returnCount != 0 ||
                result->paramCount != (contract->nullableResult ? 1 : 0)) return 0;
            if (contract->nullableResult) {
                const XL6_IrType* base = result->params ? xl6IrTypeGet(&module->types, result->params[0]) : NULL;
                if (!base || base->kind != XL6_IR_TYPE_BUILTIN || base->cAbi != contract->result ||
                    base->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED || base->paramCount ||
                    base->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL) return 0;
            }
            if (contract->result == XL6_IR_C_ABI_MANAGED_STRING || contract->result == XL6_IR_C_ABI_MANAGED_BYTES) {
                if (result->cAbi != contract->result || result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
                    result->nullAbi != (contract->result == XL6_IR_C_ABI_MANAGED_STRING
                        ? XL6_IR_NULL_ABI_POINTER_SENTINEL : XL6_IR_NULL_ABI_XVALUE_SENTINEL)) return 0;
            } else if (contract->result == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR) {
                if (!xl6IrSerializationBridgeDynamicValueType(result)) return 0;
            } else if (!xl6IrBufferRuntimeBridgeResultMatches(result, contract->result)) return 0;
        }
    }
    for (i = 0; i < contract->parameterCount; ++i) {
        const XL6_IrType* type = xl6IrTypeGet(&module->types, target->parameterTypes[i]);
        XL6_IrCAbi abi = contract->parameters[i];
        if(contract->parameterEnums[i]) {
            if(contract->parameterEnums[i]->cAbi!=abi||
                !xl6IrDirectNumericEnumMatches(module,target->parameterTypes[i],contract->parameterEnums[i])||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID||
                !xl6IrBufferRuntimeBridgeParameterMatches(module,target,i,abi,
                    XL6_IR_PARAMETER_ABI_VALUE,XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT))return 0;
            continue;
        }
        if(contract->genericValueParameter==i+1) {
            if(!type || target->parameterTypes[i]!=xl6IrNativeContextElementType(module,target) ||
               target->parameterAbis[i]!=xl6IrParameterAbiForMode(type,XL6_IR_PARAMETER_MODE_DEFAULT) ||
               target->parameterPhysicalAbis[i]!=xl6IrNativeValueParameterPhysicalAbi(module,target->parameterTypes[i]) ||
               target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
               target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->genericChannelParameter==i+1) {
            if(!type || target->parameterTypes[i]!=target->nativeContextType ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->nativeVoidCallbackArrayParameter==i+1){
            if(!type||target->parameterTypes[i]!=target->nativeContextType||
                !xl6IrNativeVoidCallbackArrayType(module,target,contract)||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->nativeClassArrayParameter==i+1) {
            if(!type || target->parameterTypes[i]!=target->nativeContextType ||
                !xl6IrNativeClassArrayContextField(module,target) ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if(contract->nativeClassParameter==i+1) {
            if(!type || target->parameterTypes[i]!=target->nativeContextType ||
                !xl6IrNativeClassContextField(module,target) ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if (contract->parameterNativeValueAbis[i]) {
            if (abi != XL6_IR_C_ABI_NATIVE_VALUE || !xl6IrNativeValuePlanForType(type) ||
                type->nativeValueAbi != contract->parameterNativeValueAbis[i] ||
                target->parameterAbis[i] != XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i] != XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID ||
                contract->parameterOwners[i] || contract->parameterDynamicBorrows[i] ||
                contract->parameterArrayItemAbis[i] || contract->parameterNullaryCallableResultAbis[i]) return 0;
            continue;
        }
        if(contract->genericFutureValueCallbackParameter==i+1){
            const XL6_IrType* future=xl6IrTypeGet(&module->types,target->returnType);
            XL6_IrTypeId element=future&&future->paramCount==1&&future->params?future->params[0]:XL6_IR_TYPE_INVALID;
            int returnsValue=type&&type->returnCount==1&&type->returns&&type->returns[0]==element&&
                type->callableResultType==element;
            int returnsVoid=type&&!type->returnCount&&!type->returns&&!type->callableResultType&&
                target->nativeContextPayloadAbi==XL6_IR_ASYNC_RESULT_VOID;
            if(!element||target->nativeContextType!=element||!type||type->kind!=XL6_IR_TYPE_FUNCTION||
                type->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR||type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED||
                type->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL||!type->typeValueId||
                type->collectionAbi!=XL6_IR_COLLECTION_NONE||type->paramCount||type->params||type->parameterAbis||
                type->parameterModes||type->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED||
                (!returnsValue&&!returnsVoid)||target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_RESULT_CLOSURE_BORROW||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID)return 0;
            continue;
        }
        if (contract->parameterNullaryCallableResultAbis[i] || contract->genericFutureStarterParameter==i+1) {
            XL6_IrCAbi resultAbi = contract->parameterNullaryCallableResultAbis[i];
            const XL6_IrType* returned=type && type->returnCount==1 && type->returns
                ? xl6IrTypeGet(&module->types,type->returns[0]):NULL;
            int resultMatches = contract->genericFutureStarterParameter==i+1
                ? returned && type->callableResultType==target->returnType &&
                    type->returns[0]==target->returnType && !contract->parameterNullaryCallableResultAbis[i]
                : resultAbi == XL6_IR_C_ABI_VOID
                ? type && !type->returnCount && !type->returns && type->callableResultType == XL6_IR_TYPE_INVALID
                : returned && type->callableResultType == type->returns[0] && returned->kind == XL6_IR_TYPE_BUILTIN &&
                  returned->cAbi == resultAbi && !returned->paramCount && !returned->returnCount &&
                  returned->collectionAbi == XL6_IR_COLLECTION_NONE &&
                  (resultAbi == XL6_IR_C_ABI_BOOL
                    ? returned->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
                      returned->nullAbi == XL6_IR_NULL_ABI_NONE &&
                      returned->integerTarget == XL6_IR_INTEGER_TARGET_INVALID
                    : resultAbi == XL6_IR_C_ABI_MANAGED_STRING &&
                      returned->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                      returned->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL);
            if (!type || type->kind!=XL6_IR_TYPE_FUNCTION || type->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR ||
                type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED || type->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
                type->collectionAbi!=XL6_IR_COLLECTION_NONE || type->paramCount || type->params ||
                type->parameterAbis || type->parameterModes || type->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED ||
                !resultMatches ||
                target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID) return 0;
            continue;
        }
        if (abi==XL6_IR_C_ABI_RUNTIME_ERROR_PTR) {
            if (!type || type->kind!=XL6_IR_TYPE_BUILTIN || type->cAbi!=abi || type->paramCount || type->returnCount ||
                type->collectionAbi!=XL6_IR_COLLECTION_NONE || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
                type->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL || target->parameterAbis[i]!=XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i]!=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i]!=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i]!=XL6_IR_NODE_INVALID) return 0;
            continue;
        }
        if (contract->parameterOwners[i]) {
            if (target->parameterAbis[i] != XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i] != XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                !xl6IrDirectBridgeResourceMatches(module, target->parameterTypes[i],
                    target->parameterPhysicalFieldDeclarations[i], 0, contract->parameterOwners[i])) return 0;
            continue;
        }
        if (contract->parameterDynamicBorrows[i]) {
            if (abi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR || !xl6IrSerializationBridgeDynamicValueType(type) ||
                target->parameterAbis[i] != XL6_IR_PARAMETER_ABI_BORROW ||
                target->parameterPhysicalAbis[i] != XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID) return 0;
            continue;
        }
        if (contract->parameterArrayItemAbis[i]) {
            if (!xl6IrDirectBridgeCollectionMatches(module, type, contract->parameterArrayItemAbis[i], XL6_IR_COLLECTION_ARRAY) ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID ||
                !xl6IrBufferRuntimeBridgeParameterMatches(module, target, i, abi,
                    XL6_IR_PARAMETER_ABI_BORROW, XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT)) return 0;
            continue;
        }
        if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && abi == XL6_IR_C_ABI_I64 &&
            (!type || type->integerTarget != XL6_IR_INTEGER_TARGET_SIGNED_64)) return 0;
        if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && abi == XL6_IR_C_ABI_U64 &&
            (!type || type->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_64)) return 0;
        if (type == NULL || type->kind != XL6_IR_TYPE_BUILTIN ||
            type->collectionAbi != XL6_IR_COLLECTION_NONE || type->paramCount != 0 ||
            target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
            target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID ||
            !xl6IrBufferRuntimeBridgeParameterMatches(module, target, i, abi,
                (abi == XL6_IR_C_ABI_MANAGED_STRING || abi == XL6_IR_C_ABI_MANAGED_BYTES)
                    ? XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE,
                XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT)) return 0;
    }
    return 1;
}
