/* The XID bridge admits only the standard nominal value and its precise
 * collection/nullable wrappers.  The emitter receives these verified TypeIds
 * and never reconstructs source declarations from a C symbol. */
typedef enum XL6_IrXidShape {
    XL6_IR_XID_SHAPE_XID = 1,
    XL6_IR_XID_SHAPE_ARRAY,
    XL6_IR_XID_SHAPE_STRING,
    XL6_IR_XID_SHAPE_BYTES,
    XL6_IR_XID_SHAPE_INT,
    XL6_IR_XID_SHAPE_BOOL
} XL6_IrXidShape;

typedef struct XL6_IrXidBridgeContract {
    const char* name;
    XL6_IrRuntimeBridgeKind kind;
    XL6_IrXidShape result;
    size_t parameterCount;
    XL6_IrXidShape parameters[2];
} XL6_IrXidBridgeContract;

static const XL6_IrXidBridgeContract xl6IrXidBridgeContracts[] = {
    {"xl6_xrt_xid_make_string", XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING,
        XL6_IR_XID_SHAPE_STRING, 0, {0}},
    {"xl6_xrt_xid_make", XL6_IR_RUNTIME_BRIDGE_XID_MAKE,
        XL6_IR_XID_SHAPE_XID, 0, {0}},
    {"xl6_xrt_xid_make_many", XL6_IR_RUNTIME_BRIDGE_XID_MAKE_MANY,
        XL6_IR_XID_SHAPE_ARRAY, 1, {XL6_IR_XID_SHAPE_INT}},
    {"xl6_xrt_xid_parse", XL6_IR_RUNTIME_BRIDGE_XID_PARSE,
        XL6_IR_XID_SHAPE_XID, 1, {XL6_IR_XID_SHAPE_STRING}},
    {"xl6_xrt_xid_from_bytes", XL6_IR_RUNTIME_BRIDGE_XID_FROM_BYTES,
        XL6_IR_XID_SHAPE_XID, 1, {XL6_IR_XID_SHAPE_BYTES}},
    {"xl6_xrt_xid_to_string", XL6_IR_RUNTIME_BRIDGE_XID_TO_STRING,
        XL6_IR_XID_SHAPE_STRING, 1, {XL6_IR_XID_SHAPE_XID}},
    {"xl6_xrt_xid_to_bytes", XL6_IR_RUNTIME_BRIDGE_XID_TO_BYTES,
        XL6_IR_XID_SHAPE_BYTES, 1, {XL6_IR_XID_SHAPE_XID}},
    {"xl6_xrt_xid_timestamp", XL6_IR_RUNTIME_BRIDGE_XID_TIMESTAMP,
        XL6_IR_XID_SHAPE_INT, 1, {XL6_IR_XID_SHAPE_XID}},
    {"xl6_xrt_xid_compare", XL6_IR_RUNTIME_BRIDGE_XID_COMPARE,
        XL6_IR_XID_SHAPE_INT, 2,
        {XL6_IR_XID_SHAPE_XID, XL6_IR_XID_SHAPE_XID}},
    {"xl6_xrt_xid_equal", XL6_IR_RUNTIME_BRIDGE_XID_EQUAL,
        XL6_IR_XID_SHAPE_BOOL, 2,
        {XL6_IR_XID_SHAPE_XID, XL6_IR_XID_SHAPE_XID}},
    {"xl6_xrt_xid_is_zero", XL6_IR_RUNTIME_BRIDGE_XID_IS_ZERO,
        XL6_IR_XID_SHAPE_BOOL, 1, {XL6_IR_XID_SHAPE_XID}}
};

static const XL6_IrXidBridgeContract* xl6IrXidBridgeContract(
    const char* name)
{
    size_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < sizeof(xl6IrXidBridgeContracts) /
                    sizeof(xl6IrXidBridgeContracts[0]); ++i)
        if (strcmp(name, xl6IrXidBridgeContracts[i].name) == 0)
            return &xl6IrXidBridgeContracts[i];
    return NULL;
}

XL6_IrRuntimeBridgeKind xl6IrRuntimeXidBridgeKind(const char* name)
{
    const XL6_IrXidBridgeContract* contract = xl6IrXidBridgeContract(name);
    return contract != NULL ? contract->kind : XL6_IR_RUNTIME_BRIDGE_NONE;
}

static int xl6IrXidTypeMatches(const XL6_IrModule* module,
    XL6_IrTypeId typeId, XL6_IrXidShape shape)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, typeId);
    if (type == NULL || type->typeValueId == 0) return 0;
    switch (shape) {
    case XL6_IR_XID_SHAPE_XID:
        return type->kind == XL6_IR_TYPE_CLASS &&
            type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
            type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            type->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
            type->collectionAbi == XL6_IR_COLLECTION_NONE &&
            type->paramCount == 0 && type->nominalSymbol != 0 &&
            type->name != NULL && strcmp(type->name, "Xid") == 0;
    case XL6_IR_XID_SHAPE_ARRAY:
        return type->collectionAbi == XL6_IR_COLLECTION_ARRAY &&
            type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
            type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            type->paramCount == 1 && type->params != NULL &&
            xl6IrXidTypeMatches(module, type->params[0],
                XL6_IR_XID_SHAPE_XID);
    case XL6_IR_XID_SHAPE_STRING:
        return type->kind == XL6_IR_TYPE_BUILTIN &&
            type->cAbi == XL6_IR_C_ABI_MANAGED_STRING;
    case XL6_IR_XID_SHAPE_BYTES:
        return type->kind == XL6_IR_TYPE_BUILTIN &&
            type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES;
    case XL6_IR_XID_SHAPE_INT:
        return type->kind == XL6_IR_TYPE_BUILTIN &&
            type->cAbi == XL6_IR_C_ABI_I64;
    case XL6_IR_XID_SHAPE_BOOL:
        return type->kind == XL6_IR_TYPE_BUILTIN &&
            type->cAbi == XL6_IR_C_ABI_BOOL;
    default: return 0;
    }
}

int xl6IrRuntimeXidBridgeTargetMatches(const XL6_IrModule* module,
    const XL6_IrCallTarget* target)
{
    const XL6_IrXidBridgeContract* contract;
    size_t i;
    if (module == NULL || target == NULL ||
        (contract = xl6IrXidBridgeContract(target->cName)) == NULL ||
        target->runtimeBridge != contract->kind ||
        target->kind != XL6_IR_CALL_TARGET_EXTERN_C ||
        target->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
         target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR) ||
        target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
        target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID ||
        target->resultPhysicalDestroySymbol != 0 ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
        target->parameterCount != contract->parameterCount ||
        !xl6IrXidTypeMatches(module, target->returnType, contract->result) ||
        (target->parameterCount != 0 &&
         (target->parameterTypes == NULL || target->parameterAbis == NULL ||
          target->parameterPhysicalAbis == NULL ||
          target->parameterConstraintAbis == NULL ||
          target->parameterPhysicalFieldDeclarations == NULL))) return 0;
    for (i = 0; i < target->parameterCount; ++i) {
        XL6_IrXidShape shape = contract->parameters[i];
        if (!xl6IrXidTypeMatches(module, target->parameterTypes[i], shape) ||
            target->parameterAbis[i] !=
                (shape == XL6_IR_XID_SHAPE_INT
                    ? XL6_IR_PARAMETER_ABI_VALUE
                    : XL6_IR_PARAMETER_ABI_BORROW) ||
            target->parameterPhysicalAbis[i] !=
                XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
            target->parameterConstraintAbis[i] !=
                XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
            target->parameterPhysicalFieldDeclarations[i] !=
                XL6_IR_NODE_INVALID) return 0;
    }
    return 1;
}
