static int xl6IrCgenRuntimeManagedXValueRetainAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* type;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->firstOperand >= module->operandCount ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    type = xl6IrTypeGet(&module->types, instruction->type);
    return source != NULL && source->type == instruction->type &&
           type != NULL &&
           ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_RETAIN &&
             type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR) ||
            (instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_RETAIN &&
             type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES)) &&
           type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
}

/* Literal projection is an opcode-to-ABI mapping shared by synchronous and
 * continuation bodies. No declaration or source expression lookup is needed. */
static const char* xl6IrCgenLiteralScalarProjection(
    XL6_IrRuntimeOp operation, XL6_IrCAbi* abi)
{
    XL6_IrCAbi result;
    const char* helper;
    switch ( operation ) {
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_I64:
        result = XL6_IR_C_ABI_I64; helper = "xlir_xvalue_object_get_literal_i64"; break;
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_U64:
        result = XL6_IR_C_ABI_U64; helper = "xlir_xvalue_object_get_literal_u64"; break;
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_BOOL:
        result = XL6_IR_C_ABI_BOOL; helper = "xlir_xvalue_object_get_literal_bool"; break;
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_F64:
        result = XL6_IR_C_ABI_F64; helper = "xlir_xvalue_object_get_literal_f64"; break;
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME:
        result = XL6_IR_C_ABI_TIME_I64; helper = "xlir_xvalue_object_get_literal_time"; break;
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_STRING_COPY:
        result = XL6_IR_C_ABI_MANAGED_STRING;
        helper = "xlir_xvalue_object_get_literal_string_copy"; break;
    default: return NULL;
    }
    if ( abi != NULL ) *abi = result;
    return helper;
}

static int xl6IrCgenSharedXValueRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return xl6IrCgenLiteralScalarProjection(operation, NULL) != NULL ||
           operation == XL6_IR_RUNTIME_XVALUE_I64 ||
           operation == XL6_IR_RUNTIME_XVALUE_RETAIN ||
           operation == XL6_IR_RUNTIME_BYTES_RETAIN ||
           operation == XL6_IR_RUNTIME_XVALUE_TYPE_BIND ||
           operation == XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED ||
           operation == XL6_IR_RUNTIME_XVALUE_STRING_COPY ||
           operation == XL6_IR_RUNTIME_XVALUE_WEAK_CREATE ||
           operation == XL6_IR_RUNTIME_XVALUE_WEAK_LOCK ||
           operation == XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE ||
           operation == XL6_IR_RUNTIME_XVALUE_ARRAY_NEW ||
           operation == XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW ||
           operation == XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW ||
           operation == XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW ||
           operation == XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE ||
           operation == XL6_IR_RUNTIME_XVALUE_OBJECT_NEW ||
           operation == XL6_IR_RUNTIME_XVALUE_CLASS_NEW ||
           operation == XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND ||
           operation == XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT ||
           operation == XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW ||
           operation == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW ||
           operation == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL ||
           operation ==
               XL6_IR_RUNTIME_XVALUE_SAFE_CLASS_FIELD_GET_LITERAL ||
           operation == XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW ||
           operation == XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL ||
           (operation >= XL6_IR_RUNTIME_TAGGED_ENUM_NEW &&
            operation <= XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT) ||
           operation ==
               XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW;
}

static int xl6IrCgenTaggedEnumRuntimeAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrEnumVariant* variant;
    const XL6_IrEnumPayload* payload = NULL;
    const XL6_IrInstruction* first;
    const XL6_IrInstruction* second;
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL ||
         instruction->enumVariant >= module->enumVariantCount ) return 0;
    variant = &module->enumVariants[instruction->enumVariant];
    if ( instruction->enumPayload != SIZE_MAX ) {
        if ( instruction->enumPayload < variant->firstPayload ||
             instruction->enumPayload >=
                 variant->firstPayload + variant->payloadCount ) return 0;
        payload = &module->enumPayloads[instruction->enumPayload];
    }
    first = instruction->operandCount > 0
        ? xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand], NULL) : NULL;
    second = instruction->operandCount > 1
        ? xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + 1], NULL) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_TAGGED_ENUM_NEW )
        return instruction->operandCount == 0 && payload == NULL &&
            instruction->type == variant->ownerType && resultType != NULL &&
            resultType->kind == XL6_IR_TYPE_TAGGED_ENUM &&
            resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    if ( instruction->runtimeOp ==
             XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_SET_NEW )
        return instruction->operandCount == 2 && payload != NULL &&
            first != NULL && first->type == variant->ownerType &&
            second != NULL &&
            xl6IrTypeGet(&module->types, second->type) != NULL &&
            xl6IrTypeGet(&module->types, second->type)->cAbi ==
                XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
            resultType != NULL && resultType->cAbi == XL6_IR_C_ABI_BOOL;
    if ( instruction->runtimeOp ==
             XL6_IR_RUNTIME_TAGGED_ENUM_TAG_EQUAL )
        return instruction->operandCount == 1 && payload == NULL &&
            first != NULL && first->type == variant->ownerType &&
            resultType != NULL && resultType->cAbi == XL6_IR_C_ABI_BOOL;
    if ( instruction->runtimeOp !=
             XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT ||
         instruction->operandCount != 1 || payload == NULL || first == NULL ||
         first->type != variant->ownerType || instruction->type != payload->type ||
         resultType == NULL ) return 0;
    return resultType->cAbi == XL6_IR_C_ABI_BOOL ||
           resultType->cAbi == XL6_IR_C_ABI_I64 ||
           resultType->cAbi == XL6_IR_C_ABI_U64 ||
           resultType->cAbi == XL6_IR_C_ABI_F64 ||
           resultType->cAbi == XL6_IR_C_ABI_TIME_I64 ||
           resultType->cAbi == XL6_IR_C_ABI_MANAGED_STRING ||
           resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
}

static int xl6IrCgenSharedXValueRuntimeAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         instruction->result == XL6_IR_VALUE_INVALID ||
         xl6IrCgenType(module, instruction->type) == NULL ) return 0;
    {
        XL6_IrCAbi abi;
        if ( xl6IrCgenLiteralScalarProjection(instruction->runtimeOp, &abi) != NULL ) {
            const XL6_IrInstruction* source = instruction->operandCount == 1 &&
                    instruction->firstOperand < module->operandCount
                ? xl6IrCgenValueDefinition(module,
                    module->operands[instruction->firstOperand], NULL) : NULL;
            const XL6_IrType* sourceType = source != NULL
                ? xl6IrTypeGet(&module->types, source->type) : NULL;
            const XL6_IrType* resultType = xl6IrTypeGet(&module->types, instruction->type);
            return instruction->immediateText != NULL && sourceType != NULL &&
                sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                resultType != NULL && (resultType->cAbi == abi ||
                    (abi == XL6_IR_C_ABI_F64 &&
                     (resultType->cAbi == XL6_IR_C_ABI_C_FLOAT ||
                      resultType->cAbi == XL6_IR_C_ABI_C_DOUBLE)));
        }
    }
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_BYTES_RETAIN:
        return xl6IrCgenRuntimeManagedXValueRetainAbi(module, instruction);
    case XL6_IR_RUNTIME_XVALUE_TYPE_BIND:
    case XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED:
        {
            const XL6_IrInstruction* source = instruction->operandCount == 1
                ? xl6IrCgenValueDefinition(module,
                    module->operands[instruction->firstOperand], NULL) : NULL;
            const XL6_IrType* sourceType = source != NULL
                ? xl6IrTypeGet(&module->types, source->type) : NULL;
            const XL6_IrType* resultType = xl6IrTypeGet(
                &module->types, instruction->type);
            const XL6_IrType* identity = xl6IrTypeGet(
                &module->types, instruction->runtimeIdentityType);
            return instruction->immediateText == NULL && identity != NULL &&
                identity->typeValueId != 0 && sourceType != NULL &&
                resultType != NULL &&
                sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
        }
    case XL6_IR_RUNTIME_TAGGED_ENUM_NEW:
    case XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_SET_NEW:
    case XL6_IR_RUNTIME_TAGGED_ENUM_TAG_EQUAL:
    case XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT:
        return xl6IrCgenTaggedEnumRuntimeAbi(module, instruction);
    case XL6_IR_RUNTIME_XVALUE_I64:
    case XL6_IR_RUNTIME_XVALUE_STRING_COPY:
        return instruction->immediateText == NULL &&
               instruction->operandCount == 1;
    case XL6_IR_RUNTIME_XVALUE_WEAK_CREATE:
        {
            const XL6_IrInstruction* source = instruction->operandCount == 1 &&
                    instruction->firstOperand < module->operandCount
                ? xl6IrCgenValueDefinition(
                    module, module->operands[instruction->firstOperand], NULL)
                : NULL;
            const XL6_IrType* sourceType = source != NULL
                ? xl6IrTypeGet(&module->types, source->type) : NULL;
            const XL6_IrType* resultType =
                xl6IrTypeGet(&module->types, instruction->type);
            return instruction->immediateText == NULL &&
                   instruction->operandCount == 1 &&
                   instruction->targetSymbol == 0 &&
                   instruction->targetValue == XL6_IR_VALUE_INVALID &&
                   instruction->calleeType == XL6_IR_TYPE_INVALID &&
                   instruction->payloadType == XL6_IR_TYPE_INVALID &&
                   instruction->opcode == XL6_IR_OPCODE_INVALID &&
                   sourceType != NULL &&
                   sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                   sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                   sourceType->kind != XL6_IR_TYPE_WEAK &&
                   resultType != NULL &&
                   resultType->kind == XL6_IR_TYPE_WEAK &&
                   resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                   resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                   resultType->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
                   resultType->paramCount == 1 &&
                   resultType->params != NULL &&
                   resultType->params[0] == source->type;
        }
    case XL6_IR_RUNTIME_XVALUE_WEAK_LOCK:
        {
            const XL6_IrInstruction* source = instruction->operandCount == 1 &&
                    instruction->firstOperand < module->operandCount
                ? xl6IrCgenValueDefinition(
                    module, module->operands[instruction->firstOperand], NULL)
                : NULL;
            const XL6_IrType* sourceType = source != NULL
                ? xl6IrTypeGet(&module->types, source->type) : NULL;
            const XL6_IrType* resultType =
                xl6IrTypeGet(&module->types, instruction->type);
            return instruction->immediateText == NULL &&
                   instruction->operandCount == 1 &&
                   instruction->targetSymbol == 0 &&
                   instruction->targetValue == XL6_IR_VALUE_INVALID &&
                   instruction->calleeType == XL6_IR_TYPE_INVALID &&
                   instruction->payloadType == XL6_IR_TYPE_INVALID &&
                   instruction->opcode == XL6_IR_OPCODE_INVALID &&
                   sourceType != NULL &&
                   sourceType->kind == XL6_IR_TYPE_WEAK &&
                   sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                   sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                   sourceType->paramCount == 1 && sourceType->params != NULL &&
                   resultType != NULL &&
                   resultType->kind == XL6_IR_TYPE_NULLABLE &&
                   resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                   resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                   resultType->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
                   resultType->paramCount == 1 &&
                   resultType->params != NULL &&
                   resultType->params[0] == sourceType->params[0];
        }
    case XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE:
        return instruction->immediateText == NULL &&
               instruction->operandCount == 1 &&
               instruction->targetSymbol != 0;
    case XL6_IR_RUNTIME_XVALUE_ARRAY_NEW:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_NEW:
    case XL6_IR_RUNTIME_XVALUE_CLASS_NEW:
        {
            const XL6_IrType* resultType =
                xl6IrTypeGet(&module->types, instruction->type);
            return instruction->immediateText == NULL &&
                   instruction->operandCount == 0 && resultType != NULL &&
                   resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                   resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
        }
    case XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND:
        return xl6IrCgenRuntimeClassIdentityBindAbi(module, instruction);
    case XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT:
        return xl6IrCgenRuntimeClassConstructionCommitAbi(module, instruction);
    case XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW:
        return instruction->immediateText == NULL &&
               instruction->operandCount == 2;
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW:
    case XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE:
        {
            if (instruction->runtimeOp==XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE)
                return xl6IrListIndexRuntimeInstructionCompatible(module,instruction);
            const XL6_IrInstruction* map = instruction->operandCount == 3
                ? xl6IrCgenValueDefinition(
                    module, module->operands[instruction->firstOperand], NULL)
                : NULL;
            const XL6_IrInstruction* key = instruction->operandCount == 3
                ? xl6IrCgenValueDefinition(
                    module, module->operands[instruction->firstOperand + 1], NULL)
                : NULL;
            const XL6_IrInstruction* value = instruction->operandCount == 3
                ? xl6IrCgenValueDefinition(
                    module, module->operands[instruction->firstOperand + 2], NULL)
                : NULL;
            const XL6_IrType* mapType = map != NULL
                ? xl6IrTypeGet(&module->types, map->type) : NULL;
            const XL6_IrType* keyType = key != NULL
                ? xl6IrTypeGet(&module->types, key->type) : NULL;
            const XL6_IrType* valueType = value != NULL
                ? xl6IrTypeGet(&module->types, value->type) : NULL;
            const XL6_IrType* resultType =
                xl6IrTypeGet(&module->types, instruction->type);
            return instruction->immediateText == NULL && mapType != NULL &&
                mapType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                keyType != NULL && keyType->cAbi == XL6_IR_C_ABI_I64 &&
                valueType != NULL &&
                valueType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                resultType != NULL && resultType->cAbi == XL6_IR_C_ABI_BOOL;
        }
    case XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW:
        return instruction->immediateText != NULL &&
               instruction->operandCount == 2;
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL:
        {
            const XL6_IrInstruction* object = instruction->operandCount == 1
                ? xl6IrCgenValueDefinition(
                    module, module->operands[instruction->firstOperand], NULL)
                : NULL;
            const XL6_IrType* objectType = object != NULL
                ? xl6IrTypeGet(&module->types, object->type) : NULL;
            const XL6_IrType* resultType =
                xl6IrTypeGet(&module->types, instruction->type);
            return instruction->immediateText != NULL && objectType != NULL &&
                objectType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                resultType != NULL &&
                resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
        }
    case XL6_IR_RUNTIME_XVALUE_SAFE_CLASS_FIELD_GET_LITERAL:
        {
            const XL6_IrInstruction* object = instruction->operandCount == 1
                ? xl6IrCgenValueDefinition(
                    module, module->operands[instruction->firstOperand], NULL)
                : NULL;
            const XL6_IrType* objectType = object != NULL
                ? xl6IrTypeGet(&module->types, object->type) : NULL;
            const XL6_IrType* classType = xl6IrTypeGet(
                &module->types, instruction->runtimeIdentityType);
            const XL6_IrType* payloadType = xl6IrTypeGet(
                &module->types, instruction->payloadType);
            const XL6_IrType* resultType = xl6IrTypeGet(
                &module->types, instruction->type);
            return instruction->immediateText != NULL &&
                   instruction->operandCount == 1 &&
                   instruction->targetSymbol == 0 &&
                   instruction->targetValue == XL6_IR_VALUE_INVALID &&
                   instruction->opcode == XL6_IR_OPCODE_INVALID &&
                   instruction->calleeType == XL6_IR_TYPE_INVALID &&
                   objectType != NULL &&
                   objectType->kind == XL6_IR_TYPE_NULLABLE &&
                   objectType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                   objectType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                   objectType->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
                   objectType->paramCount == 1 &&
                   objectType->params != NULL &&
                   objectType->params[0] ==
                       instruction->runtimeIdentityType &&
                   classType != NULL && classType->kind == XL6_IR_TYPE_CLASS &&
                   classType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                   classType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
                   classType->typeValueId != 0 && payloadType != NULL &&
                   resultType != NULL &&
                   resultType->kind == XL6_IR_TYPE_BUILTIN &&
                   resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
                   resultType->collectionAbi == XL6_IR_COLLECTION_NONE &&
                   resultType->paramCount == 0 &&
                   resultType->nominalSymbol == XL6_SYMBOL_ID_INVALID &&
                   resultType->typeValueId ==
                       module->runtimeTypeMap.fallbackTypeValueId;
        }
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW:
        return instruction->immediateText != NULL &&
               instruction->operandCount == 1;
    default:
        return 0;
    }
}

/* Return 1 when emitted, 0 when this is another runtime operation, and -1
 * when an operation in this family violates its already-verified ABI. */
static int xl6IrCgenEmitSharedXValueRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if ( instruction == NULL ||
         !xl6IrCgenSharedXValueRuntimeOpIs(instruction->runtimeOp) )
        return 0;
    if ( !xl6IrCgenSharedXValueRuntimeAbi(module, instruction) ) return -1;
    {
        const char* helper = xl6IrCgenLiteralScalarProjection(instruction->runtimeOp, NULL);
        if ( helper != NULL ) {
            xl6OutputPrintf(out, "    v%u = %s(v%u, xlir_literal_view(%s));\n",
                (unsigned int)instruction->result, helper,
                (unsigned int)module->operands[instruction->firstOperand],
                instruction->immediateText);
            return 1;
        }
    }
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_BYTES_RETAIN:
        xl6OutputPrintf(out, "    v%u = xrtValueRetain(v%u);\n",
            (unsigned)instruction->result, (unsigned)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_RUNTIME_XVALUE_TYPE_BIND:
    case XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED:
        xl6OutputPrintf(out,
            "    v%u = %s(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result,
            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED
                ? "xlir_xvalue_type_rebind_owned" : "xlir_xvalue_type_bind",
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned long long)xl6IrTypeGet(
                &module->types, instruction->runtimeIdentityType)->typeValueId);
        break;
    case XL6_IR_RUNTIME_TAGGED_ENUM_NEW:
        xl6OutputPrintf(out,
            "    v%u = xlir_tagged_enum_new(INT64_C(%u));\n",
            (unsigned int)instruction->result,
            (unsigned int)module->enumVariants[
                instruction->enumVariant].tag);
        break;
    case XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_SET_NEW:
        xl6OutputPrintf(out,
            "    v%u = xrtValueObjectSetNew(v%u, xrtStrView(\"$xlir.enum.payload.%u\"), v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)(instruction->enumPayload -
                module->enumVariants[instruction->enumVariant].firstPayload),
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_RUNTIME_TAGGED_ENUM_TAG_EQUAL:
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_object_get_literal_i64(v%u, xrtStrView(\"$xlir.enum.variant\")) == INT64_C(%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->enumVariants[
                instruction->enumVariant].tag);
        break;
    case XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT:
        {
            const XL6_IrType* payloadType = xl6IrTypeGet(
                &module->types, instruction->type);
            const char* helper = payloadType->cAbi == XL6_IR_C_ABI_BOOL
                ? "xlir_xvalue_object_get_literal_bool"
                : payloadType->cAbi == XL6_IR_C_ABI_I64
                ? "xlir_xvalue_object_get_literal_i64"
                : payloadType->cAbi == XL6_IR_C_ABI_U64
                ? "xlir_xvalue_object_get_literal_u64"
                : payloadType->cAbi == XL6_IR_C_ABI_F64
                ? "xlir_xvalue_object_get_literal_f64"
                : payloadType->cAbi == XL6_IR_C_ABI_TIME_I64
                ? "xlir_xvalue_object_get_literal_time"
                : payloadType->cAbi == XL6_IR_C_ABI_MANAGED_STRING
                ? "xlir_xvalue_object_get_literal_string_copy" : NULL;
            unsigned int payloadOffset = (unsigned int)(
                instruction->enumPayload -
                module->enumVariants[instruction->enumVariant].firstPayload);
            if ( helper != NULL ) {
                xl6OutputPrintf(out,
                    "    v%u = %s(v%u, xrtStrView(\"$xlir.enum.payload.%u\"));\n",
                    (unsigned int)instruction->result, helper,
                    (unsigned int)module->operands[instruction->firstOperand],
                    payloadOffset);
            } else {
                xl6OutputPrintf(out,
                    "    v%u = xrtValueRetain(xrtValueObjectGet(v%u, xrtStrView(\"$xlir.enum.payload.%u\")));\n"
                    "    if (v%u == NULL) abort();\n",
                    (unsigned int)instruction->result,
                    (unsigned int)module->operands[instruction->firstOperand],
                    payloadOffset, (unsigned int)instruction->result);
            }
        }
        break;
    case XL6_IR_RUNTIME_XVALUE_I64:
        xl6OutputPrintf(out, "    v%u = xrtValueInt(v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_RUNTIME_XVALUE_STRING_COPY:
        xl6OutputPrintf(out,
            "    v%u = xrtValueString(v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_RUNTIME_XVALUE_WEAK_CREATE:
        xl6OutputPrintf(out, "    v%u = xlir_xvalue_weak_create(v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_RUNTIME_XVALUE_WEAK_LOCK:
        xl6OutputPrintf(out, "    v%u = xlir_xvalue_weak_lock(v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE:
        xl6OutputPrintf(out,
            "    v%u = xlir_native_handle_take_s%u(&v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)instruction->targetSymbol,
            (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_RUNTIME_XVALUE_ARRAY_NEW:
        xl6OutputPrintf(out, "    v%u = xrtValueArray();\n",
                        (unsigned int)instruction->result);
        break;
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW:
        xl6OutputPrintf(out, "    v%u = xrtValueIntMap();\n",
                        (unsigned int)instruction->result);
        break;
    case XL6_IR_RUNTIME_XVALUE_OBJECT_NEW:
        xl6OutputPrintf(out, "    v%u = xrtValueObject();\n",
                        (unsigned int)instruction->result);
        break;
    case XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW:
        xl6OutputPrintf(out,
            "    v%u = xl6ArrayValueAppendTakeV1(v%u, v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW:
        xl6OutputPrintf(out,
            "    v%u = xrtValueIntMapSetNew(v%u, v%u, v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->operands[instruction->firstOperand + 1],
            (unsigned int)module->operands[instruction->firstOperand + 2]);
        break;
    case XL6_IR_RUNTIME_XVALUE_CLASS_NEW:
        {
            const XL6_IrClassLayout* layout = xl6IrCgenClassLayout(module, instruction->type);
            if (layout == NULL || module->abi.classStorageAbi != XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2 ||
                module->abi.classIdentityAbi != XL6_IR_CLASS_IDENTITY_RESIDENT_SCHEMA_V3)
                return 0; /* The independent IR verifier owns this diagnostic. */
            xl6OutputPrintf(out, "    v%u = xl6ClassRuntimeV3Get()->Create(%s, ",
                (unsigned int)instruction->result,
                module->abi.codeOwnerAbi == XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT
                    ? "(xlir_code_owner.size != 0 ? &xlir_code_owner : NULL)" : "NULL");
            if (layout->destroyFunction != 0)
                xl6OutputPrintf(out, "xlir_class_destroy_finalizer_t%u", (unsigned int)instruction->type);
            else xl6OutputPuts("NULL", out);
            xl6OutputPrintf(out, ", &xlir_class_identity_schema_t%u);\n",
                (unsigned int)instruction->type);
        }
        break;
    case XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND:
        xl6OutputPrintf(out,
            "    v%u = xl6ClassRuntimeV3Get()->BindIdentityTake(v%u);\n",
            (unsigned int)instruction->result,
             (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT:
        xl6OutputPrintf(out,
            "    v%u = xl6ClassRuntimeV3Get()->CommitTake(v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand]);
        break;
    case XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE:
        xl6OutputPrintf(out,"    v%u = xl6ListValueSetTakeV1(v%u, v%u, v%u);\n",
            (unsigned)instruction->result,(unsigned)module->operands[instruction->firstOperand],
            (unsigned)module->operands[instruction->firstOperand+1],
            (unsigned)module->operands[instruction->firstOperand+2]);
        break;
    case XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW:
        xl6OutputPrintf(out,
            "    v%u = xrtValueObjectSetNewPostCommit(v%u, xlir_literal_view(%s), v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            instruction->immediateText,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW:
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_dynamic_field_set_new(v%u, xlir_literal_view(%s), v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            instruction->immediateText,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL:
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_object_get_literal_or_null(v%u, xlir_literal_view(%s));\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            instruction->immediateText);
        break;
    case XL6_IR_RUNTIME_XVALUE_SAFE_CLASS_FIELD_GET_LITERAL:
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_safe_class_field_get_or_null(v%u, xlir_literal_view(%s));\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            instruction->immediateText);
        break;
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL:
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_dynamic_field_get_or_null(v%u, xlir_literal_view(%s));\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            instruction->immediateText);
        break;
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW:
        xl6OutputPrintf(out,
            "    v%u = xlir_xvalue_object_get_literal_native_handle(v%u, xlir_literal_view(%s));\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            instruction->immediateText);
        break;
    default:
        return 0;
    }
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static const char* xl6IrCgenSharedStringCompareRelation(XL6_IrRuntimeOp operation)
{
    switch (operation) {
    case XL6_IR_RUNTIME_STRING_EQUAL: return "==";
    case XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL: return "==";
    case XL6_IR_RUNTIME_STRING_ORDER_LT: return "<";
    case XL6_IR_RUNTIME_STRING_ORDER_LE: return "<=";
    case XL6_IR_RUNTIME_STRING_ORDER_GT: return ">";
    case XL6_IR_RUNTIME_STRING_ORDER_GE: return ">=";
    default: return NULL;
    }
}

static int xl6IrCgenEmitNullableScalarEqual(XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if (instruction->runtimeOp!=XL6_IR_RUNTIME_NULLABLE_SCALAR_EQUAL) return 0;
    if (!xl6IrNullableScalarEqualSignatureValid(module,instruction)) return -1;
    unsigned left=(unsigned)module->operands[instruction->firstOperand];
    unsigned right=(unsigned)module->operands[instruction->firstOperand+1];
    xl6OutputPrintf(out,"    v%u = v%u.hasValue == v%u.hasValue && (!v%u.hasValue || v%u.value == v%u.value);\n",
        (unsigned)instruction->result,left,right,left,left,right);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenSharedStringCompareAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    return xl6IrStringCompareRuntimeSignatureValid(module,instruction);
}

static int xl6IrCgenEmitSharedStringCompare(
    XL6_Output* out, const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const char* relation;
    if (instruction == NULL ||
        (relation = xl6IrCgenSharedStringCompareRelation(instruction->runtimeOp)) == NULL) return 0;
    if (!xl6IrCgenSharedStringCompareAbi(module, instruction)) return -1;
    if (instruction->runtimeOp == XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL)
        xl6OutputPrintf(out, "    v%u = xl6NullableStringEqualV1(v%u, v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->operands[instruction->firstOperand + 1]);
    else if (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_EQUAL)
        xl6OutputPrintf(out, "    v%u = xrtStrEqual(v%u, v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->operands[instruction->firstOperand + 1]);
    else
        xl6OutputPrintf(out, "    v%u = xrtStrCompare(v%u, v%u) %s 0;\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->operands[instruction->firstOperand + 1], relation);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenSharedScalarStringRuntimeOpIs(
    XL6_IrRuntimeOp operation)
{
    return operation == XL6_IR_RUNTIME_BOOL_TO_STRING ||
           operation == XL6_IR_RUNTIME_I64_TO_STRING ||
           operation == XL6_IR_RUNTIME_U64_TO_STRING ||
           operation == XL6_IR_RUNTIME_F64_TO_STRING ||
           operation == XL6_IR_RUNTIME_XVALUE_TO_STRING;
}

static int xl6IrCgenSharedScalarStringRuntimeAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    XL6_IrCAbi expected;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenSharedScalarStringRuntimeOpIs(instruction->runtimeOp) ||
         instruction->result == XL6_IR_VALUE_INVALID ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->firstOperand >= module->operandCount ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    expected = instruction->runtimeOp == XL6_IR_RUNTIME_BOOL_TO_STRING
        ? XL6_IR_C_ABI_BOOL
        : instruction->runtimeOp == XL6_IR_RUNTIME_I64_TO_STRING
            ? XL6_IR_C_ABI_I64
            : instruction->runtimeOp == XL6_IR_RUNTIME_U64_TO_STRING
                ? XL6_IR_C_ABI_U64
                : instruction->runtimeOp == XL6_IR_RUNTIME_F64_TO_STRING
                    ? XL6_IR_C_ABI_F64 : XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    return sourceType != NULL && sourceType->cAbi == expected &&
        sourceType->ownershipAbi ==
            (expected == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR
                ? XL6_IR_OWNERSHIP_ABI_MANAGED : XL6_IR_OWNERSHIP_ABI_TRIVIAL) &&
        resultType != NULL && resultType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
}

/* Scalar formatting is a backend-neutral XLIR operation family.  Synchronous
 * and continuation programs must consume the same validated ABI and emitter. */
static int xl6IrCgenEmitSharedScalarStringRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const char* helper;
    if ( instruction == NULL ||
         !xl6IrCgenSharedScalarStringRuntimeOpIs(instruction->runtimeOp) )
        return 0;
    if ( !xl6IrCgenSharedScalarStringRuntimeAbi(module, instruction) ) return -1;
    helper = instruction->runtimeOp == XL6_IR_RUNTIME_BOOL_TO_STRING
        ? "xlir_bool_to_string"
        : instruction->runtimeOp == XL6_IR_RUNTIME_I64_TO_STRING
            ? "xlir_i64_to_string"
            : instruction->runtimeOp == XL6_IR_RUNTIME_U64_TO_STRING
                ? "xlir_u64_to_string"
                : instruction->runtimeOp == XL6_IR_RUNTIME_F64_TO_STRING
                    ? "xlir_f64_to_string" : "xlir_xvalue_to_string";
    if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_STRING)
        xl6OutputPrintf(out, "    v%u = xl6ValueTextV1(v%u);\n",
            (unsigned int)instruction->result,
            (unsigned int)module->operands[instruction->firstOperand]);
    else
        xl6OutputPrintf(out, "    v%u = xlir_string_take_cstr(%s(v%u));\n",
            (unsigned int)instruction->result, helper,
            (unsigned int)module->operands[instruction->firstOperand]);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

/* Continuation programs execute after the module storage declarations have
 * been emitted.  An ordinary global/extern-global access is therefore a stable
 * module binding, not a frame capture.  Keep this partition explicit so the
 * machine verifier never mistakes shared storage for a launch parameter or a
 * spill slot.  Cell-backed globals continue to use the dedicated CELL_* ABI. */
static const XL6_IrStorage* xl6IrCgenSharedStorage(
    const XL6_IrModule* module, XL6_SymbolId symbol)
{
    const XL6_IrStorage* storage = xl6IrStorageGet(module, symbol);
    if ( storage == NULL || storage->cellType != XL6_IR_TYPE_INVALID ||
         (storage->kind != XL6_IR_STORAGE_GLOBAL &&
          storage->kind != XL6_IR_STORAGE_EXTERN_GLOBAL) ) return NULL;
    return storage;
}
