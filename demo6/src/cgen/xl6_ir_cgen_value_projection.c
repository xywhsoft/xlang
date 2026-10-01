static int xl6IrCgenCollectionXValueTag(XL6_IrCollectionAbi abi)
{
    switch ( abi ) {
    case XL6_IR_COLLECTION_ARRAY: return 9;
    case XL6_IR_COLLECTION_LIST: return 10;
    case XL6_IR_COLLECTION_SET: return 11;
    case XL6_IR_COLLECTION_DICT: return 12;
    default: return 0;
    }
}

static int xl6IrCgenPayloadXValueTag(const XL6_IrType* payload)
{
    if ( payload == NULL ) return 0;
    switch ( payload->cAbi ) {
    case XL6_IR_C_ABI_BOOL: return 1;
    case XL6_IR_C_ABI_I64: return 2;
    case XL6_IR_C_ABI_F64: return 3;
    case XL6_IR_C_ABI_MANAGED_STRING: return 4;
    case XL6_IR_C_ABI_U64: return 13;
    default: return 0;
    }
}

/* Module verification has already proved whether a source is open dynamic or
 * statically shaped erased storage and whether its shape matches the checked
 * operation.  Cgen validates only the closed physical instruction ABI needed
 * to select/render the helper; it must not repeat semantic type admission. */
static int xl6IrCgenRuntimeCollectionCheckedCastAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         xl6IrCgenCollectionXValueTag(instruction->collectionAbi) == 0 ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return sourceType != NULL && resultType != NULL &&
           sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           resultType->collectionAbi == instruction->collectionAbi &&
           resultType->paramCount == 0;
}

static int xl6IrCgenRuntimeCollectionCheckedRebindAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    const XL6_IrType* payloadType;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    payloadType = xl6IrTypeGet(&module->types, instruction->payloadType);
    return sourceType != NULL && resultType != NULL && payloadType != NULL &&
           sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           sourceType->paramCount == 0 &&
           resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           resultType->paramCount == 1 && resultType->params != NULL &&
           resultType->params[0] == instruction->payloadType &&
           resultType->collectionAbi == instruction->collectionAbi &&
           xl6IrCgenCollectionXValueTag(instruction->collectionAbi) != 0 &&
           xl6IrCgenPayloadXValueTag(payloadType) != 0;
}

/* Iterator semantics are a closed RuntimeOp family.  Both synchronous and
 * continuation emitters call this one mechanical projection table; neither
 * backend path recovers container kind, key layout or item ownership. */
static int xl6IrCgenXValueIteratorRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    switch ( operation ) {
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ADVANCE:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_I64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_ORDINAL_I64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_U64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BOOL:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_F64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BORROW:
        return 1;
    default:
        return 0;
    }
}

static int xl6IrCgenRuntimeCheckedIteratorCreateAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    const XL6_IrType* logicalType;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         instruction->runtimeOp !=
             XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED ||
         instruction->operandCount != 1 ||
         instruction->firstOperand >= module->operandCount ||
         instruction->immediateText != NULL ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    logicalType = xl6IrTypeGet(
        &module->types, instruction->payloadType);
    return sourceType != NULL &&
        sourceType->kind == XL6_IR_TYPE_BUILTIN &&
        sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        sourceType->collectionAbi == XL6_IR_COLLECTION_NONE &&
        logicalType != NULL &&
        logicalType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        logicalType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        (instruction->payloadType == source->type ||
         xl6IrCgenCollectionXValueTag(logicalType->collectionAbi) != 0) &&
        resultType != NULL &&
        resultType->kind == XL6_IR_TYPE_VALUE_ITERATOR &&
        resultType->cAbi ==
            XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR &&
        resultType->paramCount == 1 && resultType->params != NULL &&
        resultType->params[0] == source->type;
}

static int xl6IrCgenEmitXValueIteratorInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    XL6_IrValueId operand;
    const char* helper;
    if ( out == NULL || module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenXValueIteratorRuntimeOpIs(instruction->runtimeOp) ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->firstOperand >= module->operandCount ) return 0;
    operand = module->operands[instruction->firstOperand];
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE:
        helper = "xlir_xvalue_iterator_create";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED:
        if ( !xl6IrCgenRuntimeCheckedIteratorCreateAbi(
                 module, instruction) ) return 0;
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_iterator_create_checked(v%u, %d);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            xl6IrCgenCollectionXValueTag(
                xl6IrTypeGet(&module->types,
                    instruction->payloadType)->collectionAbi));
        return !xl6OutputHasFailed(out);
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ADVANCE:
        helper = "xlir_xvalue_iterator_advance";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_I64:
        helper = "xlir_xvalue_iterator_item_i64";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_ORDINAL_I64:
        helper = "xlir_xvalue_iterator_key_ordinal_i64";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64:
        helper = "xlir_xvalue_iterator_key_i64";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_U64:
        helper = "xlir_xvalue_iterator_item_u64";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BOOL:
        helper = "xlir_xvalue_iterator_item_bool";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_F64:
        helper = "xlir_xvalue_iterator_item_f64";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_STRING_COPY:
        helper = "xlir_xvalue_iterator_item_string_copy";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_XVALUE_RETAIN:
        helper = "xlir_xvalue_iterator_item_xvalue_retain";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY:
        helper = "xlir_xvalue_iterator_key_string_copy";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE:
        helper = "xlir_xvalue_iterator_key_dynamic_xvalue";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE:
        helper = "xlir_xvalue_iterator_dynamic_logical_xvalue";
        break;
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BORROW:
        helper = "xlir_xvalue_iterator_item_borrow";
        break;
    default:
        return 0;
    }
    xl6OutputPrintf(out, "    v%u = %s(v%u);\n",
        (unsigned int)instruction->result, helper, (unsigned int)operand);
    return 1;
}

static int xl6IrCgenXValueProjectionRuntimeOpIs(
    XL6_IrRuntimeOp operation)
{
    return operation == XL6_IR_RUNTIME_XVALUE_PROJECT_TAGGED_NULLABLE_COPY ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_STRING_COPY ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_CLOSURE_COPY ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_UNION_COPY ||
           operation ==
               XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_ERROR_REF ||
           operation == XL6_IR_RUNTIME_XVALUE_CAST_CHECKED ||
           operation == XL6_IR_RUNTIME_XVALUE_UNION_CAST_CHECKED ||
           operation == XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED ||
           operation == XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_TAGGED ||
           operation == XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_STRING ||
           operation == XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST ||
           operation == XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED ||
           operation == XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_CLOSURE_COPY ||
           operation == XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY ||
           operation ==
               XL6_IR_RUNTIME_XVALUE_PROJECT_NATIVE_HANDLE_BORROW;
}

static int xl6IrCgenXValueArithmeticRuntimeOpcode(
    XL6_IrRuntimeOp operation, size_t* arity)
{
    int opcode;
    switch ( operation ) {
    case XL6_IR_RUNTIME_XVALUE_BINARY_ADD: opcode = 0; break;
    case XL6_IR_RUNTIME_XVALUE_BINARY_SUB: opcode = 1; break;
    case XL6_IR_RUNTIME_XVALUE_BINARY_MUL: opcode = 2; break;
    case XL6_IR_RUNTIME_XVALUE_BINARY_DIV: opcode = 3; break;
    case XL6_IR_RUNTIME_XVALUE_BINARY_MOD: opcode = 4; break;
    case XL6_IR_RUNTIME_XVALUE_BINARY_BIT_AND: opcode = 5; break;
    case XL6_IR_RUNTIME_XVALUE_BINARY_BIT_OR: opcode = 6; break;
    case XL6_IR_RUNTIME_XVALUE_BINARY_BIT_XOR: opcode = 7; break;
    case XL6_IR_RUNTIME_XVALUE_BINARY_SHL: opcode = 8; break;
    case XL6_IR_RUNTIME_XVALUE_BINARY_SHR: opcode = 9; break;
    case XL6_IR_RUNTIME_XVALUE_UNARY_POS: opcode = 0; break;
    case XL6_IR_RUNTIME_XVALUE_UNARY_NEG: opcode = 1; break;
    case XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT: opcode = 2; break;
    default: return -1;
    }
    if ( arity != NULL )
        *arity = operation >= XL6_IR_RUNTIME_XVALUE_UNARY_POS &&
                 operation <= XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT
            ? 1u : 2u;
    return opcode;
}

static int xl6IrCgenXValueArithmeticRuntimeAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    size_t arity = 0;
    const XL6_IrType* result;
    if ( module == NULL || instruction == NULL ||
         xl6IrCgenXValueArithmeticRuntimeOpcode(
             instruction->runtimeOp, &arity) < 0 ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         instruction->operandCount != arity ||
         instruction->immediateText != NULL ||
         instruction->result == XL6_IR_VALUE_INVALID ) return 0;
    result = xl6IrTypeGet(&module->types, instruction->type);
    return result != NULL &&
           result->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           result->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
}

static int xl6IrCgenEmitXValueArithmeticRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    size_t arity = 0;
    int opcode = instruction != NULL
        ? xl6IrCgenXValueArithmeticRuntimeOpcode(
            instruction->runtimeOp, &arity) : -1;
    if ( out == NULL || opcode < 0 ||
         !xl6IrCgenXValueArithmeticRuntimeAbi(
             module, instruction) ) return 0;
    if ( arity == 1 )
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_unary(v%u, %d);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            opcode);
    else
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_binary(v%u, v%u, %d);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->operands[
                instruction->firstOperand + 1], opcode);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenRuntimeCheckedUnionCastAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);
static int xl6IrCgenRuntimeCheckedClosureCastAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);

static int xl6IrCgenTypedDictLiteralRuntimeOpIs(
    XL6_IrRuntimeOp operation)
{
    return operation == XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_I64 ||
           operation == XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_U64 ||
           operation == XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_BOOL ||
           operation == XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_F64 ||
           operation ==
               XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_STRING_COPY;
}

/* The verifier has already fixed the projected result TypeId and its payload
 * or callable metadata, including whether a closure is nullable.  Both
 * synchronous and continuation paths render exactly that operation; neither
 * sees the collection/index/loop which produced the borrowed xvalue operand. */
static int xl6IrCgenEmitXValueProjectionInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    XL6_IrValueId operand;
    const XL6_IrType* result;
    const XL6_IrType* identity;
    uint64 exactTypeValueId = 0;
    if ( out == NULL || module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenXValueProjectionRuntimeOpIs(instruction->runtimeOp) ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->firstOperand >= module->operandCount ) return 0;
    operand = module->operands[instruction->firstOperand];
    result = xl6IrTypeGet(&module->types, instruction->type);
    if ( result == NULL ) return 0;
    if ( instruction->runtimeOp ==
         XL6_IR_RUNTIME_XVALUE_PROJECT_TAGGED_NULLABLE_COPY ) {
        if ( result->cName[0] == '\0' ) return 0;
        xl6OutputPrintf(out, "    v%u = xlir_xvalue_project_%s(v%u);\n",
            (unsigned int)instruction->result, result->cName,
            (unsigned int)operand);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_STRING_COPY ) {
        xl6OutputPrintf(out,
            "    bool projection_ok_%u = xlir_xvalue_project_nullable_string_copy(v%u, &v%u);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned int)instruction->result);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_CLOSURE_COPY ) {
        xl6OutputPrintf(out,
            "    bool projection_ok_%u = xlir_xvalue_project_closure_try_copy_into(v%u, (void (*)(void))xlir_xvalue_closure_entry_t%u, true, &v%u);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned int)instruction->calleeType, (unsigned int)instruction->result);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_UNION_COPY ) {
        identity = xl6IrTypeGet(
            &module->types, instruction->payloadType);
        if ( identity == NULL || identity->typeValueId == 0 ) return 0;
        xl6OutputPrintf(out,
            "    v%u.hasValue = false;\n"
            "    if (xrtValueType(v%u) != 0) { xlir_xvalue_union_unbox(v%u, 0x%016llXULL, &v%u.value, sizeof(v%u.value)); v%u.hasValue = true; }\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned int)operand,
            (unsigned long long)identity->typeValueId,
            (unsigned int)instruction->result,
            (unsigned int)instruction->result,
            (unsigned int)instruction->result);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN ) {
        identity = xl6IrTypeGet(&module->types, instruction->payloadType);
        if ( identity == NULL || identity->typeValueId == 0 ) return 0;
        if ( instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ) {
            identity = xl6IrTypeGet(
                &module->types, instruction->runtimeIdentityType);
            if ( identity == NULL || identity->typeValueId == 0 ) return 0;
            exactTypeValueId = identity->typeValueId;
        }
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_project_nullable_retain(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned long long)exactTypeValueId);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN ) {
        identity = xl6IrTypeGet(&module->types, instruction->payloadType);
        if ( identity == NULL || instruction->payloadType !=
                 instruction->type || identity->typeValueId == 0 ) return 0;
        if ( instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ) {
            identity = xl6IrTypeGet(
                &module->types, instruction->runtimeIdentityType);
            if ( identity == NULL || identity->typeValueId == 0 ) return 0;
            exactTypeValueId = identity->typeValueId;
        }
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_project_retain(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned long long)exactTypeValueId);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY ) {
        const char* getter = result->cAbi == XL6_IR_C_ABI_BOOL
            ? "xrtValueGetBool"
            : result->cAbi == XL6_IR_C_ABI_CHAR_U32
                ? "xrtValueGetChar"
            : result->cAbi == XL6_IR_C_ABI_I64
                ? "xrtValueGetInt"
                : result->cAbi == XL6_IR_C_ABI_U64
                    ? "xrtValueGetUInt"
                    : result->cAbi == XL6_IR_C_ABI_F64
                        ? "xrtValueGetFloat"
                        : result->cAbi == XL6_IR_C_ABI_TIME_I64
                            ? "xrtValueGetTime" : NULL;
        if ( getter == NULL || instruction->payloadType != instruction->type ||
             result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
             result->nullAbi != XL6_IR_NULL_ABI_NONE ) return 0;
        xl6OutputPrintf(out,
            "    if (v%u == NULL || !%s(v%u, &v%u)) abort();\n",
            (unsigned int)operand, getter, (unsigned int)operand,
            (unsigned int)instruction->result);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW ) {
        if ( instruction->payloadType != instruction->type ||
             result->cAbi != XL6_IR_C_ABI_RAW_POINTER ||
             result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
             result->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL ) return 0;
        xl6OutputPrintf(out,
            "    if (v%u == NULL || !xrtValueGetPointer(v%u, &v%u)) abort();\n",
            (unsigned int)operand, (unsigned int)operand,
            (unsigned int)instruction->result);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY ) {
        if ( instruction->payloadType != instruction->type ||
             result->cAbi != XL6_IR_C_ABI_MANAGED_STRING ||
             result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
             result->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL ) return 0;
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_project_string_copy(v%u);\n",
            (unsigned int)instruction->result, (unsigned int)operand);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_ERROR_REF ) {
        identity = xl6IrTypeGet(&module->types, instruction->payloadType);
        if ( instruction->payloadType != instruction->type ||
             instruction->runtimeIdentityType != instruction->type ||
             identity == NULL || identity->typeValueId == 0 ||
             identity->cAbi != XL6_IR_C_ABI_RUNTIME_ERROR_PTR ||
             identity->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
             identity->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL ) return 0;
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_project_error_ref(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned long long)identity->typeValueId);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_CAST_CHECKED ) {
        int matchingResult =
            (instruction->castAbi ==
                 XL6_IR_CAST_XVALUE_TO_BOOL_CHECKED &&
             result->cAbi == XL6_IR_C_ABI_BOOL) ||
            (instruction->castAbi ==
                 XL6_IR_CAST_XVALUE_TO_I64_CHECKED &&
             result->cAbi == XL6_IR_C_ABI_I64) ||
            (instruction->castAbi ==
                 XL6_IR_CAST_XVALUE_TO_U64_CHECKED &&
             result->cAbi == XL6_IR_C_ABI_U64) ||
            (instruction->castAbi ==
                 XL6_IR_CAST_XVALUE_TO_F64_CHECKED &&
             result->cAbi == XL6_IR_C_ABI_F64) ||
            (instruction->castAbi ==
                 XL6_IR_CAST_XVALUE_TO_TIME_CHECKED &&
             result->cAbi == XL6_IR_C_ABI_TIME_I64) ||
            (instruction->castAbi ==
                 XL6_IR_CAST_XVALUE_TO_STRING_CHECKED &&
             result->cAbi == XL6_IR_C_ABI_MANAGED_STRING) ||
            (instruction->castAbi == XL6_IR_CAST_XVALUE_TO_BYTES_CHECKED &&
             result->cAbi == XL6_IR_C_ABI_MANAGED_BYTES &&
             result->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED) ||
            (instruction->castAbi ==
                 XL6_IR_CAST_XVALUE_TO_RAW_POINTER_CHECKED &&
             result->cAbi == XL6_IR_C_ABI_RAW_POINTER &&
             result->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
             result->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL) ||
            (instruction->castAbi ==
                 XL6_IR_CAST_XVALUE_TO_ERROR_CHECKED &&
             result->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
             result->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
             result->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL);
        if ( instruction->payloadType != instruction->type ||
             result->kind == XL6_IR_TYPE_NULLABLE ||
             result->typeValueId == 0 || !matchingResult ) return 0;
        xl6OutputPrintf(out,
            "    (void)xlir_xvalue_cast_checked(v%u, 0x%016llXULL, %d, &v%u);\n",
            (unsigned int)operand,
            (unsigned long long)result->typeValueId,
            (int)instruction->castAbi,
            (unsigned int)instruction->result);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_UNION_CAST_CHECKED ) {
        identity = xl6IrTypeGet(&module->types, instruction->payloadType);
        if ( identity == NULL || identity->cName[0] == '\0' ||
             !xl6IrCgenRuntimeCheckedUnionCastAbi(
                 module, instruction) ) return 0;
        xl6OutputPrintf(out,
            "    v%u = (%s){0};\n"
            "    (void)xlir_xvalue_union_cast_checked(v%u, 0x%016llXULL, &v%u, sizeof(v%u));\n",
            (unsigned int)instruction->result, identity->cName,
            (unsigned int)operand,
            (unsigned long long)identity->typeValueId,
            (unsigned int)instruction->result,
            (unsigned int)instruction->result);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED ) {
        identity = xl6IrTypeGet(&module->types, instruction->payloadType);
        if ( identity == NULL ||
             !xl6IrCgenRuntimeCheckedClosureCastAbi(
                 module, instruction) ) return 0;
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_cast_closure_checked(v%u, 0x%016llXULL, (void (*)(void))xlir_xvalue_closure_entry_t%u);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned long long)identity->typeValueId,
            (unsigned int)instruction->calleeType);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_TAGGED ) {
        identity = xl6IrTypeGet(&module->types, instruction->payloadType);
        if ( result->kind != XL6_IR_TYPE_NULLABLE ||
             result->cAbi != XL6_IR_C_ABI_TAGGED_NULLABLE ||
             result->nullAbi != XL6_IR_NULL_ABI_TAGGED_VALUE ||
             result->cName[0] == '\0' || identity == NULL ||
             identity->typeValueId == 0 ) return 0;
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_try_cast_%s(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, result->cName,
            (unsigned int)operand,
            (unsigned long long)identity->typeValueId);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_STRING ) {
        identity = xl6IrTypeGet(&module->types, instruction->payloadType);
        if ( result->kind != XL6_IR_TYPE_NULLABLE ||
             result->cAbi != XL6_IR_C_ABI_MANAGED_STRING ||
             result->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL ||
             result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
             identity == NULL || identity->typeValueId == 0 ) return 0;
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_try_cast_nullable_string(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned long long)identity->typeValueId);
    } else if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST ||
                instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED ||
                instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE ) {
        XL6_IrNominalIdentitySetId setId =
            XL6_IR_NOMINAL_IDENTITY_SET_INVALID;
        const XL6_IrNominalIdentitySet* set =
            xl6IrNominalIdentitySetGet(
                module, instruction->payloadType, &setId);
        if ( set == NULL || setId == XL6_IR_NOMINAL_IDENTITY_SET_INVALID )
            return 0;
        if ( instruction->runtimeOp ==
             XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST ) {
            xl6OutputPrintf(out,
                "    v%u = xlir_nominal_set_%u_matches(v%u);\n",
                (unsigned int)instruction->result, (unsigned int)setId,
                (unsigned int)operand);
        } else if ( instruction->runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED ) {
            xl6OutputPrintf(out,
                "    v%u = NULL;\n"
                "    if (xlir_nominal_set_%u_matches(v%u)) v%u = xrtValueRetain(v%u);\n"
                "    else if (xrtGetError() == NULL) xrtSetErrorTake(xlir_error_create(\"TypeError\", \"dynamic value does not match requested nominal target\"));\n",
                (unsigned int)instruction->result, (unsigned int)setId,
                (unsigned int)operand, (unsigned int)instruction->result,
                (unsigned int)operand);
        } else {
            xl6OutputPrintf(out,
                "    v%u = xlir_nominal_set_%u_matches(v%u) ? xrtValueRetain(v%u) : xrtValueRetain(xrtValueNull());\n",
                (unsigned int)instruction->result, (unsigned int)setId,
                (unsigned int)operand, (unsigned int)operand);
        }
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_NATIVE_HANDLE_BORROW ) {
        identity = xl6IrTypeGet(
            &module->types, instruction->runtimeIdentityType);
        if ( identity == NULL || instruction->runtimeIdentityType !=
                 instruction->type ||
             identity->kind != XL6_IR_TYPE_CLASS ||
             identity->cAbi != XL6_IR_C_ABI_NATIVE_HANDLE ||
             identity->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
             identity->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL ||
             identity->typeValueId == 0 ) return 0;
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_project_native_handle_borrow(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned long long)identity->typeValueId);
    } else if ( instruction->runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY ) {
        if ( result->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
             result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
             result->paramCount != 1 || result->params == NULL ||
             result->params[0] != instruction->payloadType ||
             xl6IrTypeGet(
                 &module->types, instruction->payloadType) == NULL ) return 0;
        xl6OutputPrintf(out,
            "    v%u = xrtValueGetFuture(v%u);\n"
            "    if (v%u == NULL || (v%u = xrtFutureRef(v%u)) == NULL) abort();\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned int)instruction->result,
            (unsigned int)instruction->result,
            (unsigned int)instruction->result);
    } else {
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_project_closure_try_copy(v%u, (void (*)(void))xlir_xvalue_closure_entry_t%u);\n",
            (unsigned int)instruction->result, (unsigned int)operand,
            (unsigned int)instruction->calleeType);
    }
    return 1;
}
