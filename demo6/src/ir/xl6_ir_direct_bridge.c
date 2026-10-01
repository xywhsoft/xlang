/* Included by xl6_ir.c after the canonical ABI predicates.  A direct bridge
 * has no hidden receiver, callback result, native field, or variadic contract.
 * Both lowering and verification consume this table; Cgen consumes only IDs. */
typedef struct XL6_IrDirectBridgeContract {
    const char* name;
    XL6_IrRuntimeBridgeKind kind;
    XL6_IrCAbi result;
    size_t parameterCount;
    XL6_IrCAbi parameters[6];
    XL6_IrCAbi resultArrayItemAbi;
    XL6_IrIntegerTarget resultIntegerTarget;
    XL6_IrCAbi parameterArrayItemAbis[6];
} XL6_IrDirectBridgeContract;

static const XL6_IrDirectBridgeContract xl6IrDirectBridgeContracts[] = {
#define S XL6_IR_C_ABI_MANAGED_STRING
#define I XL6_IR_C_ABI_I64
#define T XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE
#define B XL6_IR_C_ABI_BOOL
#define C XL6_IR_C_ABI_CHAR_U32
#define V XL6_IR_C_ABI_RUNTIME_XVALUE_PTR
#define Y XL6_IR_C_ABI_MANAGED_BYTES
#define F XL6_IR_C_ABI_F64
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
    {"xl6TextCharsV1",T,V,2,{S,I},C},
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
    {"xl6TextSplitV1",T,V,4,{S,S,I,I},S},
    {"xl6TextLinesV1",T,V,2,{S,I},S},
    {"xl6TextFieldsV1",T,V,2,{S,I},S},
    {"xl6TextJoinV1",T,S,4,{V,S,I,I},.parameterArrayItemAbis={S}},
#undef F
#undef Y
#undef V
#undef C
#undef B
#undef S
#undef I
#undef T
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

static int xl6IrDirectBridgeArrayMatches(const XL6_IrModule* module,
    const XL6_IrType* type, XL6_IrCAbi itemAbi)
{
    const XL6_IrType* item = type && type->paramCount == 1 && type->params
        ? xl6IrTypeGet(&module->types, type->params[0]) : NULL;
    if (!type || type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        type->collectionAbi != XL6_IR_COLLECTION_ARRAY || type->returnCount != 0 ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL || !item ||
        item->kind != XL6_IR_TYPE_BUILTIN || item->paramCount != 0 ||
        item->collectionAbi != XL6_IR_COLLECTION_NONE || item->returnCount != 0 ||
        item->cAbi != itemAbi) return 0;
    if (itemAbi == XL6_IR_C_ABI_MANAGED_STRING)
        return item->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            item->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL;
    return xl6IrBufferRuntimeBridgeResultMatches(item, itemAbi);
}

int xl6IrRuntimeDirectBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrDirectBridgeContract* contract;
    const XL6_IrType* result;
    size_t i;
    if (module == NULL || target == NULL || target->kind != XL6_IR_CALL_TARGET_EXTERN_C ||
        (contract = xl6IrDirectBridgeContract(target->cName)) == NULL ||
        target->runtimeBridge != contract->kind || target->errorAbi !=
            (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE) ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
         target->errorPhysicalAbi != (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE
            ? XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR : XL6_IR_CALL_ERROR_PHYSICAL_NONE)) ||
        target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
        target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID ||
        target->resultPhysicalDestroySymbol != 0 ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
        target->parameterCount != contract->parameterCount ||
        target->parameterTypes == NULL || target->parameterAbis == NULL ||
        target->parameterPhysicalAbis == NULL || target->parameterConstraintAbis == NULL ||
        target->parameterPhysicalFieldDeclarations == NULL) return 0;
    result = xl6IrTypeGet(&module->types, target->returnType);
    if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE) {
        const XL6_IrNode* declaration = xl6IrNodeGet(module, target->declaration);
        if (target->effectAbi != XL6_IR_CALL_EFFECT_CONSERVATIVE ||
            target->executionBoundaryAbi != XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK ||
            !declaration || (declaration->flags & XL6_AST_FLAG_FOREIGN_NO_RAISE)) return 0;
        if (contract->result == XL6_IR_C_ABI_I64 && (!result ||
            result->integerTarget != (contract->resultIntegerTarget ? contract->resultIntegerTarget
                : XL6_IR_INTEGER_TARGET_SIGNED_64))) return 0;
    }
    if (contract->result == XL6_IR_C_ABI_VOID) {
        if (target->returnType != XL6_IR_TYPE_INVALID) return 0;
    } else {
        if (contract->resultArrayItemAbi) {
            if (!xl6IrDirectBridgeArrayMatches(module, result, contract->resultArrayItemAbi)) return 0;
        } else {
            if (result == NULL || result->kind != XL6_IR_TYPE_BUILTIN ||
                result->collectionAbi != XL6_IR_COLLECTION_NONE || result->paramCount != 0) return 0;
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
        if (contract->parameterArrayItemAbis[i]) {
            if (!xl6IrDirectBridgeArrayMatches(module, type, contract->parameterArrayItemAbis[i]) ||
                target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
                target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID ||
                !xl6IrBufferRuntimeBridgeParameterMatches(module, target, i, abi,
                    XL6_IR_PARAMETER_ABI_BORROW, XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT)) return 0;
            continue;
        }
        if (contract->kind == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && abi == XL6_IR_C_ABI_I64 &&
            (!type || type->integerTarget != XL6_IR_INTEGER_TARGET_SIGNED_64)) return 0;
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
