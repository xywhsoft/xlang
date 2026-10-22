/* Included by xl6_ir.c after the shared physical-parameter validators.
 * The signature alphabet describes IR facts, not C text:
 * H native-class handle borrow, A array borrow, S string borrow, I int64 value,
 * U uint16 language integer (int64 physical ABI), Z c_size, P raw pointer.
 * Result V void, B bool, I int64, P raw pointer, F future<class>, C native TAKE.
 * These components keep FOREIGN_SCOPE observation and transactional argument /
 * result rollback. Provider membership does not imply XRT_THREAD_ERROR ABI.
 * No prefix matching: adding a runtime entry requires an explicit contract. */
typedef struct XL6_IrNetDatagramContract {
    const char* name;
    XL6_IrRuntimeBridgeKind kind;
    const char* parameters;
    char result;
    XL6_IrRuntimeBridgeKind receiverRole;
} XL6_IrNetDatagramContract;
#define NET_ROW(name, role, args, result) { "xl6_xrt_" name, XL6_IR_RUNTIME_BRIDGE_NET_##role, args, result, XL6_IR_RUNTIME_BRIDGE_NET_##role }
static const XL6_IrNetDatagramContract xl6IrNetDatagramContracts[] = {
    NET_ROW("net_runtime_stats", ENGINE_STATS, "", 'P'),
};
#undef NET_ROW
static const XL6_IrNetDatagramContract* xl6IrNetDatagramContract(const char* name)
{
    size_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < sizeof(xl6IrNetDatagramContracts) / sizeof(xl6IrNetDatagramContracts[0]); ++i)
        if (strcmp(name, xl6IrNetDatagramContracts[i].name) == 0) return &xl6IrNetDatagramContracts[i];
    return NULL;
}
XL6_IrRuntimeBridgeKind xl6IrRuntimeNetDatagramBridgeKind(const char* name)
{
    const XL6_IrNetDatagramContract* contract = xl6IrNetDatagramContract(name);
    return contract != NULL ? contract->kind : XL6_IR_RUNTIME_BRIDGE_NONE;
}
int xl6IrRuntimeNetDatagramBridgeKindIs(XL6_IrRuntimeBridgeKind kind)
{
    return kind >= XL6_IR_RUNTIME_BRIDGE_NET_ADDRESS_LIST && kind <= XL6_IR_RUNTIME_BRIDGE_NET_UDP_STATS;
}
/* Native representation identity comes from the owning field's destructor
 * contract, never the class spelling or a guessed runtime object shape. */
const XL6_IrClassField* xl6IrRuntimeNetNativeField(const XL6_IrModule* module,
    XL6_IrTypeId type, XL6_IrRuntimeBridgeKind destroyRole)
{
    const XL6_IrClassLayout* layout = xl6IrBuildClassLayoutByType(module, type);
    const XL6_IrType* owner = module != NULL ? xl6IrTypeGet(&module->types, type) : NULL;
    const XL6_IrClassField* field;
    const XL6_IrType* storage;
    const XL6_IrCallTarget* destroy;
    if (layout == NULL || owner == NULL || owner->kind != XL6_IR_TYPE_CLASS ||
        owner->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR || owner->typeValueId == 0 ||
        layout->runtimeTypeTag == 0 || layout->fieldCount != 1 || layout->firstField >= module->classFieldCount) return NULL;
    field = &module->classFields[layout->firstField];
    storage = xl6IrTypeGet(&module->types, field->type);
    destroy = xl6IrCallTargetGet(module, field->nativeDestroySymbol);
    if (field->ownerType != type || field->name == NULL || !field->name[0] ||
        storage == NULL || storage->cAbi != XL6_IR_C_ABI_RAW_POINTER || destroy == NULL ||
        destroy->runtimeBridge != destroyRole || destroy->returnType != XL6_IR_TYPE_INVALID ||
        destroy->parameterCount != 1 || destroy->parameterTypes == NULL || destroy->parameterTypes[0] != type ||
        !xl6IrBufferRuntimeBridgeParameterMatches(module, destroy, 0, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
            XL6_IR_PARAMETER_ABI_BORROW, XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW) ||
        destroy->parameterPhysicalFieldDeclarations == NULL || destroy->parameterPhysicalFieldDeclarations[0] != field->declaration)
        return NULL;
    return field;
}
const XL6_IrClassField* xl6IrRuntimeNetBufferField(const XL6_IrModule* module)
{
    const XL6_IrClassField* result = NULL;
    size_t i;
    if (module == NULL) return NULL;
    for (i = 0; i < module->classLayoutCount; ++i) {
        const XL6_IrClassField* field = xl6IrRuntimeNetNativeField(module, module->classLayouts[i].type,
            XL6_IR_RUNTIME_BRIDGE_BUFFER);
        const XL6_IrCallTarget* destroy = field != NULL ? xl6IrCallTargetGet(module, field->nativeDestroySymbol) : NULL;
        if (destroy == NULL || destroy->cName == NULL || strcmp(destroy->cName, "xl6BufferDestroyV1")) continue;
        if (result != NULL && result != field) return NULL;
        result = field;
    }
    return result;
}
int xl6IrRuntimeNetDatagramBridgeTargetMatches(const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrNetDatagramContract* contract;
    const XL6_IrType* result;
    size_t i;
    if (module == NULL || target == NULL || (contract = xl6IrNetDatagramContract(target->cName)) == NULL ||
        target->runtimeBridge != contract->kind || target->runtimeBridgeOperation != XL6_IR_RUNTIME_BRIDGE_OPERATION_NONE ||
        target->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE &&
         !(module->stage != XL6_IR_STAGE_LOWERED && target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_INVALID)) ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE || strlen(contract->parameters) != target->parameterCount ||
        (target->parameterCount && (target->parameterConstraintAbis == NULL || target->parameterPhysicalFieldDeclarations == NULL))) return 0;
    for (i = 0; i < target->parameterCount; ++i) {
        char parameter = contract->parameters[i];
        XL6_IrCAbi abi = parameter == 'H' || parameter == 'A' ? XL6_IR_C_ABI_RUNTIME_XVALUE_PTR :
            parameter == 'S' ? XL6_IR_C_ABI_MANAGED_STRING : parameter == 'P' ? XL6_IR_C_ABI_RAW_POINTER :
            parameter == 'Z' ? XL6_IR_C_ABI_C_SIZE : XL6_IR_C_ABI_I64;
        XL6_IrParameterAbi ownership = parameter == 'H' || parameter == 'A' || parameter == 'S' ?
            XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE;
        XL6_IrCallParameterPhysicalAbi physical = parameter == 'H' ?
            XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW : XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
        const XL6_IrType* type;
        if (!xl6IrBufferRuntimeBridgeParameterMatches(module, target, i, abi, ownership, physical) ||
            target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE) return 0;
        type = xl6IrTypeGet(&module->types, target->parameterTypes[i]);
        if (parameter == 'H' ? (type->kind != XL6_IR_TYPE_CLASS || target->parameterPhysicalFieldDeclarations[i] == XL6_IR_NODE_INVALID) :
            target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID) return 0;
        if (parameter == 'U' && type->integerTarget != XL6_IR_INTEGER_TARGET_UNSIGNED_16) return 0;
        if (parameter == 'H' && module->stage == XL6_IR_STAGE_LOWERED &&
            xl6IrRuntimeNetNativeField(module, target->parameterTypes[i], contract->receiverRole) == NULL) return 0;
        if (parameter == 'A' && type->collectionAbi != XL6_IR_COLLECTION_ARRAY) return 0;
        if (parameter == 'A' && module->stage == XL6_IR_STAGE_LOWERED &&
            target->reachability != XL6_IR_REACHABILITY_UNREACHABLE && xl6IrRuntimeNetBufferField(module) == NULL) return 0;
    }
    result = xl6IrTypeGet(&module->types, target->returnType);
    if (contract->result == 'C') {
        if (module->stage == XL6_IR_STAGE_LOWERED &&
            xl6IrRuntimeNetNativeField(module, target->returnType, XL6_IR_RUNTIME_BRIDGE_NET_ADDRESS_LIST) == NULL) return 0;
        return target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY && result != NULL && result->kind == XL6_IR_TYPE_CLASS &&
            result->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR && result->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            result->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
            target->resultPhysicalAbi == XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE &&
            target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID && target->resultPhysicalDestroySymbol != 0;
    }
    if (target->kind != XL6_IR_CALL_TARGET_EXTERN_C || target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
        target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID || target->resultPhysicalDestroySymbol != 0) return 0;
    if (contract->result == 'V') return target->returnType == XL6_IR_TYPE_INVALID;
    if (contract->result == 'F') {
        const XL6_IrType* payload;
        if (result == NULL || result->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
            result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED || result->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL ||
            result->paramCount != 1 || result->params == NULL) return 0;
        payload = xl6IrTypeGet(&module->types, result->params[0]);
        if (module->stage == XL6_IR_STAGE_LOWERED &&
            xl6IrRuntimeNetNativeField(module, result->params[0],
                contract->kind == XL6_IR_RUNTIME_BRIDGE_NET_RESOLVE_ASYNC ?
                    XL6_IR_RUNTIME_BRIDGE_NET_ADDRESS_LIST : XL6_IR_RUNTIME_BRIDGE_NET_UDP_PACKET) == NULL) return 0;
        if (module->stage == XL6_IR_STAGE_LOWERED) {
            const XL6_IrClassLayout* layout = xl6IrBuildClassLayoutByType(module, result->params[0]);
            if (layout == NULL || layout->hashAbi != XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE ||
                layout->equalityAbi != XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE) return 0;
            for (i = 0; i < module->callTargetCount; ++i) {
                const XL6_IrCallTarget* other = &module->callTargets[i];
                if (other->runtimeBridge == target->runtimeBridge && other->returnType != target->returnType) return 0;
            }
        }
        return payload != NULL && payload->kind == XL6_IR_TYPE_CLASS && payload->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
            payload->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED && payload->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL;
    }
    return xl6IrBufferRuntimeBridgeResultMatches(result, contract->result == 'I' ? XL6_IR_C_ABI_I64 :
        contract->result == 'B' ? XL6_IR_C_ABI_BOOL : XL6_IR_C_ABI_RAW_POINTER);
}
