/* Included after the common physical ABI validators. The declaration is
 * recognized once, with its complete signature. Backends consume operation
 * IDs and must never rediscover these roles from the emitted C symbol. */
typedef struct XL6_IrRandomBridgeContract {
    const char* name;
    XL6_IrRuntimeBridgeOperation operation;
    const char* parameters;
    char result;
} XL6_IrRandomBridgeContract;
#define RNG_ROW(name, op, args, result) {"xl6_xrt_" name, XL6_IR_RUNTIME_BRIDGE_OPERATION_##op, args, result}
static const XL6_IrRandomBridgeContract xl6IrRandomBridgeContracts[] = {
    RNG_ROW("rng_create", RNG_CREATE, "UU", 'C'),
    RNG_ROW("rng_create_default", RNG_CREATE_DEFAULT, "U", 'C'),
    RNG_ROW("rng_destroy", RNG_DESTROY, "H", 'V'),
    RNG_ROW("rng_seed", RNG_SEED, "HUU", 'V'),
    RNG_ROW("rng_u32", RNG_U32, "H", 'N'),
    RNG_ROW("rng_u64", RNG_U64, "H", 'U'),
    RNG_ROW("rng_below", RNG_BELOW, "HU", 'U'),
    RNG_ROW("rng_range", RNG_RANGE, "HII", 'I'),
    RNG_ROW("rng_range_closed", RNG_RANGE_CLOSED, "HII", 'I'),
    RNG_ROW("rng_real", RNG_REAL, "H", 'D'),
    RNG_ROW("rng_bytes", RNG_BYTES, "HI", 'Y'),
    RNG_ROW("rng_string", RNG_STRING, "HSZII", 'P'),
    RNG_ROW("rng_fill_buffer", RNG_FILL_BUFFER, "HP", 'B'),
    RNG_ROW("rng_shuffle_array", RNG_SHUFFLE_ARRAY, "HA", 'B'),
    RNG_ROW("rng_shuffle_buffer", RNG_SHUFFLE_BUFFER, "HP", 'B'),
    RNG_ROW("random_seed", RAND_SEED, "UU", 'V'),
    RNG_ROW("random_u32", RAND_U32, "", 'N'),
    RNG_ROW("random_u64", RAND_U64, "", 'U'),
    RNG_ROW("random_below", RAND_BELOW, "U", 'U'),
    RNG_ROW("random_range", RAND_RANGE, "II", 'I'),
    RNG_ROW("random_range_closed", RAND_RANGE_CLOSED, "II", 'I'),
    RNG_ROW("random_real", RAND_REAL, "", 'D'),
    RNG_ROW("random_bytes", RAND_BYTES, "I", 'Y'),
    RNG_ROW("random_string", RAND_STRING, "SZII", 'P'),
    RNG_ROW("random_fill_buffer", RAND_FILL_BUFFER, "P", 'B'),
    RNG_ROW("random_shuffle_array", RAND_SHUFFLE_ARRAY, "A", 'B'),
    RNG_ROW("random_shuffle_buffer", RAND_SHUFFLE_BUFFER, "P", 'B'),
    RNG_ROW("secure_bytes", SECURE_BYTES, "I", 'Y'),
    RNG_ROW("secure_fill_buffer", SECURE_FILL_BUFFER, "P", 'B'),
    RNG_ROW("secure_hex", SECURE_HEX, "I", 'P'),
    RNG_ROW("secure_string", SECURE_STRING, "SZII", 'P'),
    RNG_ROW("siphasher64_create", SIPHASHER_CREATE, "Y", 'C'),
    RNG_ROW("siphasher64_adopt", SIPHASHER_ADOPT, "P", 'C'),
    RNG_ROW("siphasher64_destroy", SIPHASHER_DESTROY, "H", 'V'),
    RNG_ROW("siphasher64_update_string", SIPHASHER_UPDATE_STRING, "HSZI", 'B'),
    RNG_ROW("siphasher64_update_bytes", SIPHASHER_UPDATE_BYTES, "HYI", 'B'),
    RNG_ROW("siphasher64_update_raw", SIPHASHER_UPDATE_RAW, "HPZI", 'B'),
    RNG_ROW("siphasher64_digest", SIPHASHER_DIGEST, "H", 'Q'),
    RNG_ROW("siphasher64_reset", SIPHASHER_RESET, "H", 'V'),
    RNG_ROW("siphasher64_clone", SIPHASHER_CLONE, "H", 'P'),
    RNG_ROW("siphasher64_input_bytes", SIPHASHER_INPUT_BYTES, "H", 'Q')
};
#undef RNG_ROW

static const XL6_IrRandomBridgeContract* xl6IrRandomBridgeContract(const char* name)
{
    size_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < sizeof(xl6IrRandomBridgeContracts)/sizeof(*xl6IrRandomBridgeContracts); ++i)
        if (strcmp(name, xl6IrRandomBridgeContracts[i].name) == 0)
            return &xl6IrRandomBridgeContracts[i];
    return NULL;
}

/* H is an exact native field borrow, A an array, Y bytes, S a string;
 * the scalar alphabet names storage ABIs, never a source type spelling. */
static XL6_IrCAbi xl6IrRandomBridgeCAbi(char code)
{
    switch (code) {
    case 'H': case 'A': return XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    case 'Y': return XL6_IR_C_ABI_MANAGED_BYTES;
    case 'S': return XL6_IR_C_ABI_MANAGED_STRING;
    case 'I': return XL6_IR_C_ABI_I64;
    case 'U': return XL6_IR_C_ABI_U64;
    case 'Q': return XL6_IR_C_ABI_C_ULONGLONG;
    case 'N': return XL6_IR_C_ABI_C_UINT;
    case 'D': return XL6_IR_C_ABI_F64;
    case 'B': return XL6_IR_C_ABI_BOOL;
    case 'Z': return XL6_IR_C_ABI_C_SIZE;
    case 'P': return XL6_IR_C_ABI_RAW_POINTER;
    default: return XL6_IR_C_ABI_VOID;
    }
}

static int xl6IrRandomBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const XL6_IrRandomBridgeContract* contract)
{
    const XL6_IrType* result;
    size_t i, count = strlen(contract->parameters);
    if (target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_NONE ||
        target->runtimeBridgeOperation != contract->operation ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE || target->parameterCount != count ||
        target->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
         target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE) ||
        (count && (!target->parameterTypes || !target->parameterAbis ||
                   !target->parameterPhysicalAbis || !target->parameterPhysicalFieldDeclarations ||
                   !target->parameterConstraintAbis))) return 0;
    result = xl6IrTypeGet(&module->types, target->returnType);
    if (contract->result == 'C') {
        if (target->kind != XL6_IR_CALL_TARGET_NATIVE_FACTORY || !result ||
            result->kind != XL6_IR_TYPE_CLASS ||
            target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE ||
            !target->resultPhysicalFieldDeclaration || !target->resultPhysicalDestroySymbol) return 0;
    } else {
        if (target->kind != XL6_IR_CALL_TARGET_EXTERN_C ||
            target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
            target->resultPhysicalFieldDeclaration || target->resultPhysicalDestroySymbol) return 0;
        if (contract->result == 'V') {
            if (target->returnType != XL6_IR_TYPE_INVALID) return 0;
        } else if (contract->result == 'Y') {
            if (!result || result->cAbi != XL6_IR_C_ABI_MANAGED_BYTES ||
                result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
                result->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL) return 0;
        } else if (!xl6IrBufferRuntimeBridgeResultMatches(result, xl6IrRandomBridgeCAbi(contract->result))) return 0;
    }
    for (i = 0; i < count; ++i) {
        char code = contract->parameters[i];
        const XL6_IrType* type = xl6IrTypeGet(&module->types, target->parameterTypes[i]);
        XL6_IrParameterAbi ownership = (code == 'H' || code == 'A' || code == 'Y' || code == 'S')
            ? XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE;
        if (!type || target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
            !xl6IrBufferRuntimeBridgeParameterMatches(module, target, i, xl6IrRandomBridgeCAbi(code),
                ownership, code == 'H' ? XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW
                                      : XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT)) return 0;
        if (code == 'H') {
            if (type->kind != XL6_IR_TYPE_CLASS ||
                xl6IrCallTargetNativeClassPhysicalField(module, target, i) == NULL) return 0;
        } else if (target->parameterPhysicalFieldDeclarations[i]) return 0;
        if (code == 'A' && type->collectionAbi != XL6_IR_COLLECTION_ARRAY) return 0;
    }
    return 1;
}
