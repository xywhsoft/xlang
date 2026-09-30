/* Included by xl6_ir.c after the canonical ABI predicates.  A direct bridge
 * has no hidden receiver, callback result, native field, or variadic contract.
 * Both lowering and verification consume this table; Cgen consumes only IDs. */
typedef struct XL6_IrDirectBridgeContract {
    const char* name;
    XL6_IrRuntimeBridgeKind kind;
    XL6_IrCAbi result;
    size_t parameterCount;
    XL6_IrCAbi parameters[5];
} XL6_IrDirectBridgeContract;

static const XL6_IrDirectBridgeContract xl6IrDirectBridgeContracts[] = {
    { "xl6_xrt_write", XL6_IR_RUNTIME_BRIDGE_TEXT_WRITE, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_writeln", XL6_IR_RUNTIME_BRIDGE_TEXT_WRITELN, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_console_error", XL6_IR_RUNTIME_BRIDGE_TEXT_ERROR, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_console_error_line", XL6_IR_RUNTIME_BRIDGE_TEXT_ERRORLN, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_console_flush", XL6_IR_RUNTIME_BRIDGE_TEXT_FLUSH, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_list", XL6_IR_RUNTIME_BRIDGE_DIR_LIST, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_glob", XL6_IR_RUNTIME_BRIDGE_DIR_GLOB, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
      4, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_entries", XL6_IR_RUNTIME_BRIDGE_DIR_ENTRIES, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
      5, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_is_empty", XL6_IR_RUNTIME_BRIDGE_DIR_IS_EMPTY, XL6_IR_C_ABI_C_BOOL,
      1, { XL6_IR_C_ABI_MANAGED_STRING } },
    { "xl6_xrt_dir_count", XL6_IR_RUNTIME_BRIDGE_DIR_COUNT, XL6_IR_C_ABI_I64,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_dir_size", XL6_IR_RUNTIME_BRIDGE_DIR_SIZE, XL6_IR_C_ABI_I64,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL } },
    /* process.exec returns an owned raw allocation; the language wrapper
     * explicitly transfers it to the existing take-cstr bridge. */
    { "xl6_xrt_process_exec", XL6_IR_RUNTIME_BRIDGE_PROCESS_EXEC, XL6_IR_C_ABI_RAW_POINTER,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_process_success", XL6_IR_RUNTIME_BRIDGE_PROCESS_SUCCESS, XL6_IR_C_ABI_C_BOOL,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_process_exit_code", XL6_IR_RUNTIME_BRIDGE_PROCESS_EXIT_CODE, XL6_IR_C_ABI_C_INT,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_INT } },
    { "xl6_xrt_hash32_string", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_UINT,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_string_seed", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_UINT,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_UINT, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_string", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_ULONGLONG,
      2, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_string_seed", XL6_IR_RUNTIME_BRIDGE_HASH_STRING, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_ULONGLONG, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_bytes", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_UINT,
      2, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_bytes_seed", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_UINT,
      3, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_C_UINT, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_bytes", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_ULONGLONG,
      2, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_bytes_seed", XL6_IR_RUNTIME_BRIDGE_HASH_BYTES, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_C_ULONGLONG, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_raw", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_UINT,
      3, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash32_raw_seed", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_UINT,
      4, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_C_UINT, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_raw", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_hash64_raw_seed", XL6_IR_RUNTIME_BRIDGE_HASH_RAW, XL6_IR_C_ABI_C_ULONGLONG,
      4, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_C_ULONGLONG, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_siphash64_string", XL6_IR_RUNTIME_BRIDGE_SIPHASH_STRING, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_siphash64_bytes", XL6_IR_RUNTIME_BRIDGE_SIPHASH_BYTES, XL6_IR_C_ABI_C_ULONGLONG,
      3, { XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } },
    { "xl6_xrt_siphash64_raw", XL6_IR_RUNTIME_BRIDGE_SIPHASH_RAW, XL6_IR_C_ABI_C_ULONGLONG,
      4, { XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_SIZE, XL6_IR_C_ABI_MANAGED_BYTES, XL6_IR_C_ABI_I64 } }
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

XL6_IrRuntimeBridgeKind xl6IrRuntimeDirectBridgeKind(const char* name)
{
    const XL6_IrDirectBridgeContract* contract = xl6IrDirectBridgeContract(name);
    return contract != NULL ? contract->kind : XL6_IR_RUNTIME_BRIDGE_NONE;
}

int xl6IrRuntimeDirectBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* contract;
    const XL6_IrType* result;
    size_t i;
    if (module == NULL || target == NULL || target->kind != XL6_IR_CALL_TARGET_EXTERN_C ||
        (contract = xl6IrDirectBridgeContract(target->cName)) == NULL ||
        target->runtimeBridge != contract->kind || target->errorAbi != XL6_IR_ERROR_ABI_NO_RAISE ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
         target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_NONE) ||
        target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
        target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID ||
        target->resultPhysicalDestroySymbol != 0 ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
        target->parameterCount != contract->parameterCount ||
        target->parameterTypes == NULL || target->parameterAbis == NULL ||
        target->parameterPhysicalAbis == NULL || target->parameterConstraintAbis == NULL ||
        target->parameterPhysicalFieldDeclarations == NULL) return 0;
    result = xl6IrTypeGet(&module->types, target->returnType);
    if (contract->result == XL6_IR_C_ABI_VOID) {
        if (target->returnType != XL6_IR_TYPE_INVALID) return 0;
    } else {
        if (result == NULL || result->kind != XL6_IR_TYPE_BUILTIN ||
            result->collectionAbi != XL6_IR_COLLECTION_NONE || result->paramCount != 0) return 0;
        if (contract->result == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR) {
            if (!xl6IrSerializationBridgeDynamicValueType(result)) return 0;
        } else if (!xl6IrBufferRuntimeBridgeResultMatches(result, contract->result)) return 0;
    }
    for (i = 0; i < contract->parameterCount; ++i) {
        const XL6_IrType* type = xl6IrTypeGet(&module->types, target->parameterTypes[i]);
        XL6_IrCAbi abi = contract->parameters[i];
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
