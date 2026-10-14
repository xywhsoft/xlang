static const XL6_IrInstruction* xl6IrCgenValueDefinition(
    const XL6_IrModule* module, XL6_IrValueId value, size_t* index)
{
    size_t i;
    if ( module == NULL || value == XL6_IR_VALUE_INVALID ) return NULL;
    for ( i = 0; i < module->instructionCount; ++i ) {
        if ( module->instructions[i].result == value ) {
            if ( index != NULL ) *index = i;
            return &module->instructions[i];
        }
    }
    return NULL;
}

static int xl6IrCgenDiscardAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* type;
    const XL6_IrNode* sourceOrigin;
    int borrowedManagedLoad;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_DISCARD ||
         instruction->result != XL6_IR_VALUE_INVALID ||
         instruction->operandCount != 1 ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    type = xl6IrTypeGet(&module->types, instruction->type);
    sourceOrigin = source != NULL ? xl6IrNodeGet(module, source->origin) : NULL;
    borrowedManagedLoad = type != NULL &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        source != NULL && source->kind == XL6_IR_INSTR_LOAD &&
        sourceOrigin != NULL &&
        sourceOrigin->ownership == XL6_IR_OWNERSHIP_BORROW &&
        sourceOrigin->valueCategory != XL6_IR_VALUE_RVALUE;
    return source != NULL && source->type == instruction->type &&
           xl6IrCgenType(module, instruction->type) != NULL &&
           (type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
            type->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR || borrowedManagedLoad);
}

static int xl6IrCgenEmitSimpleObservation(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    if ( instruction == NULL ) return 0;
    if ( instruction->kind == XL6_IR_INSTR_DISCARD ) {
        if ( !xl6IrCgenDiscardAbi(module, instruction) )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR discard has no deterministic non-owning observation ABI");
        xl6OutputPrintf(out, "    (void)v%u;\n",
            (unsigned int)module->operands[instruction->firstOperand]);
    } else if ( instruction->kind == XL6_IR_INSTR_TRAP_IF_FALSE ) {
        if ( instruction->result != XL6_IR_VALUE_INVALID ||
             instruction->operandCount != 1 )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                "XLIR C emitter runtime trap ABI is unresolved");
        xl6OutputPrintf(out, "    if (!v%u) abort();\n",
            (unsigned int)module->operands[instruction->firstOperand]);
    } else {
        return 0;
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenNullInstructionAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* type;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_NULL ||
         instruction->result == XL6_IR_VALUE_INVALID ||
         instruction->operandCount != 0 || instruction->immediateText != NULL ) return 0;
    type = xl6IrTypeGet(&module->types, instruction->type);
    return type != NULL && xl6IrCgenType(module, instruction->type) != NULL &&
        (type->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL ||
         (type->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
          (type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
           type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES)) ||
         (type->nullAbi == XL6_IR_NULL_ABI_TAGGED_VALUE &&
          (type->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE || xl6IrNullableNativeValuePlan(&module->types,type)!=NULL)));
}

static int xl6IrCgenEmitNullInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    const XL6_IrType* type;
    if ( !xl6IrCgenNullInstructionAbi(module, instruction) )
        return xl6IrCgenDiag(diags, module,
            instruction != NULL ? instruction->origin : XL6_IR_NODE_INVALID,
            "XLIR C emitter null ABI is unresolved");
    type = xl6IrTypeGet(&module->types, instruction->type);
    if ( type->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL )
        xl6OutputPrintf(out, "    v%u = xrtValueRetain(xrtValueNull());\n",
            (unsigned int)instruction->result);
    else if ( type->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL )
        xl6OutputPrintf(out, "    v%u = %s;\n",
            (unsigned int)instruction->result,
            type->cAbi == XL6_IR_C_ABI_MANAGED_STRING
                ? "(xlir_string){0}" : "NULL");
    else
        xl6OutputPrintf(out, "    v%u = (%s){ false };\n",
            (unsigned int)instruction->result,
            xl6IrCgenType(module, instruction->type));
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenNullableInstructionAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* operand;
    const XL6_IrType* operandType;
    const XL6_IrType* nullableType;
    const XL6_IrType* element;
    XL6_IrTypeId elementType;
    int taggedAbi;
    int sentinelAbi;
    if ( module == NULL || instruction == NULL ||
         instruction->operandCount != 1 ||
         instruction->firstOperand >= module->operandCount ||
         instruction->result == XL6_IR_VALUE_INVALID ) return 0;
    operand = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    operandType = operand != NULL
        ? xl6IrTypeGet(&module->types, operand->type) : NULL;
    if ( operandType == NULL ) return 0;
    if ( instruction->kind == XL6_IR_INSTR_NULL_TEST ) {
        const XL6_IrType* result = xl6IrTypeGet(
            &module->types, instruction->type);
        return result != NULL && result->cAbi == XL6_IR_C_ABI_BOOL &&
            (operandType->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL ||
             operandType->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL ||
             (operandType->nullAbi == XL6_IR_NULL_ABI_TAGGED_VALUE &&
              (operandType->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE || xl6IrNullableNativeValuePlan(&module->types,operandType)!=NULL)));
    }
    if ( instruction->kind != XL6_IR_INSTR_NULLABLE_WRAP &&
         instruction->kind != XL6_IR_INSTR_NULLABLE_UNWRAP ) return 0;
    nullableType = instruction->kind == XL6_IR_INSTR_NULLABLE_WRAP
        ? xl6IrTypeGet(&module->types, instruction->type) : operandType;
    elementType = nullableType != NULL && nullableType->paramCount == 1 &&
            nullableType->params != NULL
        ? nullableType->params[0] : XL6_IR_TYPE_INVALID;
    element = xl6IrTypeGet(&module->types, elementType);
    taggedAbi = nullableType != NULL &&
        nullableType->kind == XL6_IR_TYPE_NULLABLE &&
        ((nullableType->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE &&
        nullableType->nullAbi == XL6_IR_NULL_ABI_TAGGED_VALUE) || xl6IrNullableNativeValuePlan(&module->types,nullableType)!=NULL);
    sentinelAbi = nullableType != NULL && element != NULL &&
        nullableType->kind == XL6_IR_TYPE_NULLABLE &&
        (nullableType->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL ||
         nullableType->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL) &&
        nullableType->nullAbi == element->nullAbi &&
        nullableType->cAbi == element->cAbi &&
        nullableType->ownershipAbi == element->ownershipAbi;
    return element != NULL && (taggedAbi || sentinelAbi) &&
        (instruction->kind == XL6_IR_INSTR_NULLABLE_WRAP
            ? operand->type == elementType
            : instruction->type == elementType) &&
        xl6IrCgenType(module, instruction->type) != NULL;
}

static int xl6IrCgenEmitNullableInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags)
{
    const XL6_IrInstruction* operand;
    const XL6_IrType* operandType;
    XL6_IrValueId value;
    if ( !xl6IrCgenNullableInstructionAbi(module, instruction) )
        return xl6IrCgenDiag(diags, module,
            instruction != NULL ? instruction->origin : XL6_IR_NODE_INVALID,
            "XLIR C emitter nullable instruction ABI is unresolved");
    value = module->operands[instruction->firstOperand];
    operand = xl6IrCgenValueDefinition(module, value, NULL);
    operandType = xl6IrTypeGet(&module->types, operand->type);
    if ( instruction->kind == XL6_IR_INSTR_NULL_TEST ) {
        if ( operandType->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL )
            xl6OutputPrintf(out,
                "    v%u = (v%u == NULL || xrtValueType(v%u) == 0);\n",
                (unsigned int)instruction->result, (unsigned int)value,
                (unsigned int)value);
        else if ( operandType->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL )
            xl6OutputPrintf(out, "    v%u = (v%u%s == NULL);\n",
                (unsigned int)instruction->result, (unsigned int)value,
                operandType->cAbi == XL6_IR_C_ABI_MANAGED_STRING
                    ? ".Data" : "");
        else
            xl6OutputPrintf(out, "    v%u = !v%u.hasValue;\n",
                (unsigned int)instruction->result, (unsigned int)value);
    } else if ( instruction->kind == XL6_IR_INSTR_NULLABLE_WRAP &&
                xl6IrTypeGet(&module->types, instruction->type)->cAbi ==
                    XL6_IR_C_ABI_TAGGED_NULLABLE ) {
        xl6OutputPrintf(out, "    v%u = (%s){ true, v%u };\n",
            (unsigned int)instruction->result,
            xl6IrCgenType(module, instruction->type), (unsigned int)value);
    } else if ( instruction->kind == XL6_IR_INSTR_NULLABLE_UNWRAP &&
                (operandType->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE || xl6IrNullableNativeValuePlan(&module->types,operandType)!=NULL) ) {
        xl6OutputPrintf(out, "    v%u = v%u.value;\n",
            (unsigned int)instruction->result, (unsigned int)value);
    } else {
        if(instruction->kind==XL6_IR_INSTR_NULLABLE_WRAP &&
            xl6IrNullableNativeValuePlan(&module->types,xl6IrTypeGet(&module->types,instruction->type))){
            xl6OutputPrintf(out,"    v%u = (%s){ true, v%u };\n",(unsigned)instruction->result,
                xl6IrCgenType(module,instruction->type),(unsigned)value);
            return !xl6OutputHasFailed(out);
        }
        xl6OutputPrintf(out, "    v%u = v%u;\n",
            (unsigned int)instruction->result, (unsigned int)value);
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenRuntimeIntegerConvertAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL ) return 0;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    if ( resultType == NULL ||
         resultType->integerTarget != instruction->integerTarget ||
         instruction->integerSourceAbi <= XL6_IR_INTEGER_SOURCE_INVALID ||
         instruction->integerSourceAbi > XL6_IR_INTEGER_SOURCE_XVALUE ) return 0;
    return xl6IrCheckedIntegerConversionSignature(resultType,instruction->integerTarget,instruction->integerSourceAbi);
}

static int xl6IrCgenRuntimeCheckedFloatConvertAbi(
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
         instruction->integerTarget != XL6_IR_INTEGER_TARGET_INVALID ||
         instruction->integerSourceAbi != XL6_IR_INTEGER_SOURCE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    if (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT ||
        instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT ||
        instruction->runtimeOp == XL6_IR_RUNTIME_F64_TO_F32_EXPLICIT)
        return sourceType && resultType && instruction->castAbi == XL6_IR_CAST_INVALID &&
            resultType->cAbi == XL6_IR_C_ABI_F32 &&
            xl6IrCheckedFloatConversionSignature(module, sourceType, resultType, XL6_IR_CAST_INVALID) &&
            ((instruction->runtimeOp == XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT) ==
             (sourceType->cAbi == XL6_IR_C_ABI_MANAGED_STRING)) &&
            ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT) ==
             (sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR));
    return sourceType != NULL && resultType != NULL &&
        sourceType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        sourceType->kind != XL6_IR_TYPE_NULLABLE &&
        resultType->cAbi == XL6_IR_C_ABI_F64 &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
}

static int xl6IrCgenTypeIsOpenDynamicBuiltin(
    const XL6_IrModule* module, const XL6_IrType* type)
{
    return module != NULL && type != NULL &&
        type->kind == XL6_IR_TYPE_BUILTIN &&
        type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        type->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
        type->collectionAbi == XL6_IR_COLLECTION_NONE &&
        type->runtimeResourceAbi == XL6_IR_RUNTIME_RESOURCE_NONE &&
        type->paramCount == 0 &&
        type->nominalSymbol == XL6_SYMBOL_ID_INVALID &&
        type->typeValueId == module->runtimeTypeMap.fallbackTypeValueId;
}

static int xl6IrCgenRuntimeCheckedTimeConvertAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    int sourceMatches;
    if ( module == NULL || instruction == NULL ||
         (instruction->runtimeOp != XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED &&
          instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED) ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->integerTarget != XL6_IR_INTEGER_TARGET_INVALID ||
         instruction->integerSourceAbi != XL6_IR_INTEGER_SOURCE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    sourceMatches = instruction->runtimeOp ==
            XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED
        ? sourceType != NULL &&
          sourceType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
          sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
          sourceType->kind != XL6_IR_TYPE_NULLABLE
        : xl6IrCgenTypeIsOpenDynamicBuiltin(module, sourceType);
    return sourceMatches && resultType != NULL &&
        resultType->cAbi == XL6_IR_C_ABI_TIME_I64 &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
}

static int xl6IrCgenRuntimeTimeToStringAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_TIME_TO_STRING ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return sourceType != NULL && resultType != NULL &&
        sourceType->cAbi == XL6_IR_C_ABI_TIME_I64 &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        resultType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        resultType->kind != XL6_IR_TYPE_NULLABLE;
}

static int xl6IrCgenRuntimeTypeIdToStringAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_TYPE_ID_TO_STRING ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return sourceType != NULL && resultType != NULL &&
        sourceType->cAbi == XL6_IR_C_ABI_TYPE_ID &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        resultType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        resultType->kind != XL6_IR_TYPE_NULLABLE;
}

static int xl6IrCgenRuntimeClassDerivedStringAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    const XL6_IrClassLayout* layout;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp !=
             XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    layout = xl6IrCgenClassLayout(module, instruction->payloadType);
    return source != NULL && source->type == instruction->payloadType &&
        sourceType != NULL && sourceType->kind == XL6_IR_TYPE_CLASS &&
        sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        resultType != NULL &&
        resultType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        resultType->kind != XL6_IR_TYPE_NULLABLE && layout != NULL &&
        layout->stringAbi == XL6_IR_CLASS_STRING_ABI_RUNTIME_DERIVED &&
        layout->stringTarget == 0;
}

static int xl6IrCgenRuntimeClassDerivedEqualAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* left;
    const XL6_IrInstruction* right;
    const XL6_IrType* operandType;
    const XL6_IrType* resultType;
    const XL6_IrClassLayout* layout;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL ||
         instruction->operandCount != 2 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    left = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    right = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand + 1], NULL);
    operandType = left != NULL
        ? xl6IrTypeGet(&module->types, left->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    layout = xl6IrCgenClassLayout(module, instruction->payloadType);
    return left != NULL && right != NULL &&
        left->type == instruction->payloadType && right->type == left->type &&
        operandType != NULL && operandType->kind == XL6_IR_TYPE_CLASS &&
        operandType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        operandType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        resultType != NULL && resultType->cAbi == XL6_IR_C_ABI_BOOL &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        layout != NULL &&
        layout->equalityAbi ==
            XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_DERIVED &&
        layout->equalityTarget == 0;
}

static int xl6IrCgenRuntimeClassIdentityBindAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* type;
    const XL6_IrClassLayout* layout;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp !=
             XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    type = xl6IrTypeGet(&module->types, instruction->type);
    layout = xl6IrCgenClassLayout(module, instruction->payloadType);
    return source != NULL && source->type == instruction->type &&
        instruction->type == instruction->payloadType &&
        type != NULL && type->kind == XL6_IR_TYPE_CLASS &&
        type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        layout != NULL && layout->hashTarget == 0 &&
        layout->equalityTarget == 0 &&
        ((layout->hashAbi == XL6_IR_CLASS_HASH_ABI_RUNTIME_DERIVED &&
          layout->equalityAbi ==
              XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_DERIVED) ||
         (layout->hashAbi == XL6_IR_CLASS_HASH_ABI_RUNTIME_REFERENCE &&
          layout->equalityAbi ==
              XL6_IR_CLASS_EQUALITY_ABI_RUNTIME_REFERENCE));
}

static int xl6IrCgenRuntimeClassConstructionCommitAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* type;
    const XL6_IrClassLayout* layout;
    const XL6_IrFunction* program;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp !=
             XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    type = xl6IrTypeGet(&module->types, instruction->type);
    layout = xl6IrCgenClassLayout(module, instruction->payloadType);
    if ( source == NULL || source->type != instruction->type ||
         instruction->type != instruction->payloadType || type == NULL ||
         type->kind != XL6_IR_TYPE_CLASS ||
         type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
         type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
         layout == NULL ||
         layout->firstDestroyStep > module->classDestroyStepCount ||
         layout->destroyStepCount > module->classDestroyStepCount -
             layout->firstDestroyStep ) return 0;
    if (module->abi.classStorageAbi != XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2) return 0;
    if (layout->destroyStepCount == 0) return layout->destroyFunction == 0;
    if (layout->destroyFunction == 0 || layout->destroyFunction > module->functionCount) return 0;
    program = &module->functions[layout->destroyFunction - 1];
    return program->kind == XL6_IR_FUNCTION_CLASS_FINI && program->symbol != 0 &&
        program->declaration == layout->declaration && program->parameterCount == 1 &&
        program->returnType == XL6_IR_TYPE_INVALID && program->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE;
}

static int xl6IrCgenRuntimeTimeBoxAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TIME ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return sourceType != NULL && resultType != NULL &&
        sourceType->cAbi == XL6_IR_C_ABI_TIME_I64 &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
}

static int xl6IrCgenRuntimeIdentityBoxMetadataAbi(
    const XL6_IrInstruction* instruction, XL6_IrRuntimeOp operation)
{
    return instruction != NULL &&
        instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
        instruction->runtimeOp == operation &&
        instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_INVALID &&
        instruction->sourceAsyncResultAbi == XL6_IR_ASYNC_RESULT_INVALID &&
        instruction->futureContinuationKind ==
            XL6_IR_FUTURE_CONTINUATION_INVALID &&
        instruction->generatorYieldAbi == XL6_IR_GENERATOR_YIELD_INVALID &&
        instruction->scalarConversionAbi == XL6_IR_SCALAR_CONVERSION_INVALID &&
        instruction->castAbi == XL6_IR_CAST_INVALID &&
        instruction->callableErasureAbi == XL6_IR_CALLABLE_ERASURE_INVALID &&
        instruction->integerTarget == XL6_IR_INTEGER_TARGET_INVALID &&
        instruction->integerSourceAbi == XL6_IR_INTEGER_SOURCE_INVALID &&
        instruction->collectionAbi == XL6_IR_COLLECTION_NONE &&
        instruction->dynamicCollectionOp == XL6_IR_DYNAMIC_COLLECTION_INVALID &&
        instruction->typedCollectionOp == XL6_IR_TYPED_COLLECTION_INVALID &&
        instruction->immediateText == NULL &&
        instruction->targetValue == XL6_IR_VALUE_INVALID &&
        instruction->calleeType == XL6_IR_TYPE_INVALID &&
        instruction->payloadType == XL6_IR_TYPE_INVALID &&
        instruction->runtimeResourcePayloadAbi ==
            XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID &&
        instruction->runtimeResourceKeyType == XL6_IR_TYPE_INVALID &&
        instruction->runtimeResourceKeyPayloadAbi ==
            XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID &&
        instruction->runtimeResourceKeyOrderAbi ==
            XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID &&
        instruction->sourcePayloadType == XL6_IR_TYPE_INVALID &&
        instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID &&
        instruction->targetSymbol == 0 && instruction->dispatchSlot == 0 &&
        instruction->elementIndex == SIZE_MAX &&
        instruction->dynamicCallSite == SIZE_MAX &&
        instruction->dynamicCallResultAbi ==
            XL6_IR_DYNAMIC_CALL_RESULT_INVALID &&
        instruction->enumVariant == SIZE_MAX &&
        instruction->enumPayload == SIZE_MAX &&
        instruction->trueTarget == XL6_IR_BLOCK_INVALID &&
        instruction->falseTarget == XL6_IR_BLOCK_INVALID &&
        instruction->opcode == XL6_IR_OPCODE_INVALID &&
        instruction->operandCount == 1;
}

static int xl6IrCgenRuntimeForeignScalarBoxAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    if ( module == NULL ||
         !xl6IrCgenRuntimeIdentityBoxMetadataAbi(
             instruction, XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR) ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return source != NULL && sourceType != NULL && resultType != NULL &&
        xl6IrNativeScalarProfileForCAbi(sourceType->cAbi)!=XL6_NATIVE_SCALAR_NONE &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        sourceType->typeValueId != 0 &&
        instruction->runtimeIdentityType == source->type &&
        resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
}

static int xl6IrCgenRuntimeErrorXValueRefAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    if ( module == NULL ||
         !xl6IrCgenRuntimeIdentityBoxMetadataAbi(
             instruction, XL6_IR_RUNTIME_XVALUE_ERROR_REF) ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return source != NULL && sourceType != NULL && resultType != NULL &&
        sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        sourceType->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL &&
        sourceType->typeValueId != 0 &&
        instruction->runtimeIdentityType == source->type &&
        resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
}

static int xl6IrCgenRuntimeFutureXValueRefAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_FUTURE_REF ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return sourceType != NULL && resultType != NULL &&
        sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
        sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        sourceType->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL &&
        sourceType->paramCount == 1 && sourceType->params != NULL &&
        instruction->runtimeIdentityType ==
            (sourceType->kind == XL6_IR_TYPE_NULLABLE ? sourceType->params[0] : source->type) &&
        xl6IrFutureProjectionPayloadType(module,source->type,
            sourceType->kind == XL6_IR_TYPE_NULLABLE ? XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_FUTURE_COPY :
                XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY) != XL6_IR_TYPE_INVALID &&
        xl6IrTypeGet(&module->types, sourceType->params[0]) != NULL &&
        resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
}

static int xl6IrCgenRuntimeIntegerWrappingAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL ||
         (instruction->integerSourceAbi != XL6_IR_INTEGER_SOURCE_I64 &&
          instruction->integerSourceAbi != XL6_IR_INTEGER_SOURCE_U64) ||
         instruction->integerTarget < XL6_IR_INTEGER_TARGET_SIGNED_8 ||
         instruction->integerTarget > XL6_IR_INTEGER_TARGET_UNSIGNED_64 ) return 0;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return resultType != NULL &&
        resultType->integerTarget == instruction->integerTarget &&
        (instruction->integerTarget == XL6_IR_INTEGER_TARGET_UNSIGNED_64
            ? resultType->cAbi == XL6_IR_C_ABI_U64
            : resultType->cAbi == XL6_IR_C_ABI_I64);
}

static int xl6IrCgenCheckedIntegerRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation == XL6_IR_RUNTIME_INTEGER_DIV_CHECKED ||
           operation == XL6_IR_RUNTIME_INTEGER_MOD_CHECKED ||
           operation == XL6_IR_RUNTIME_INTEGER_SHL_CHECKED ||
           operation == XL6_IR_RUNTIME_INTEGER_SHR_CHECKED;
}

static int xl6IrCgenRuntimeI64PowAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* left;
    const XL6_IrInstruction* right;
    const XL6_IrType* type;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_I64_POW ||
         instruction->operandCount != 2 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->integerTarget != XL6_IR_INTEGER_TARGET_INVALID ||
         instruction->integerSourceAbi != XL6_IR_INTEGER_SOURCE_INVALID ) return 0;
    left = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    right = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand + 1], NULL);
    type = xl6IrTypeGet(&module->types, instruction->type);
    return left != NULL && right != NULL && type != NULL &&
        left->type == instruction->type && right->type == instruction->type &&
        type->cAbi == XL6_IR_C_ABI_I64 &&
        type->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_64 &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
}

static int xl6IrCgenRuntimeTimeApproxAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* left;
    const XL6_IrInstruction* right;
    const XL6_IrType* resultType;
    const XL6_IrType* leftType;
    const XL6_IrType* rightType;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_TIME_APPROX ||
         instruction->operandCount != 2 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->integerTarget != XL6_IR_INTEGER_TARGET_INVALID ||
         instruction->integerSourceAbi != XL6_IR_INTEGER_SOURCE_INVALID ) return 0;
    left = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    right = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand + 1], NULL);
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    leftType = left != NULL ? xl6IrTypeGet(&module->types, left->type) : NULL;
    rightType = right != NULL ? xl6IrTypeGet(&module->types, right->type) : NULL;
    return left != NULL && right != NULL && resultType != NULL &&
        leftType != NULL && rightType != NULL && left->type == right->type &&
        resultType->cAbi == XL6_IR_C_ABI_BOOL &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        leftType->cAbi == XL6_IR_C_ABI_TIME_I64 &&
        rightType->cAbi == XL6_IR_C_ABI_TIME_I64 &&
        leftType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        rightType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
}

static int xl6IrCgenRuntimeSpecialF64Abi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         (instruction->runtimeOp != XL6_IR_RUNTIME_F64_NAN &&
          instruction->runtimeOp != XL6_IR_RUNTIME_F64_POSITIVE_INFINITY) ||
         instruction->operandCount != 0 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->integerTarget != XL6_IR_INTEGER_TARGET_INVALID ||
         instruction->integerSourceAbi != XL6_IR_INTEGER_SOURCE_INVALID ) return 0;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return resultType != NULL && resultType->cAbi == XL6_IR_C_ABI_F64 &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
}

static int xl6IrCgenRuntimeCheckedIntegerArithmeticAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* left;
    const XL6_IrInstruction* right;
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenCheckedIntegerRuntimeOpIs(instruction->runtimeOp) ||
         instruction->operandCount != 2 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->integerTarget < XL6_IR_INTEGER_TARGET_SIGNED_8 ||
         instruction->integerTarget > XL6_IR_INTEGER_TARGET_UNSIGNED_64 ||
         instruction->integerSourceAbi != XL6_IR_INTEGER_SOURCE_INVALID ) return 0;
    left = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    right = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand + 1], NULL);
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return left != NULL && right != NULL && resultType != NULL &&
        left->type == instruction->type && right->type == instruction->type &&
        resultType->integerTarget == instruction->integerTarget &&
        resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        resultType->cAbi ==
            (instruction->integerTarget == XL6_IR_INTEGER_TARGET_UNSIGNED_64
                ? XL6_IR_C_ABI_U64 : XL6_IR_C_ABI_I64);
}

static int xl6IrCgenEmitCheckedIntegerArithmetic(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const char* helper;
    int operation;
    if ( !xl6IrCgenRuntimeCheckedIntegerArithmeticAbi(module, instruction) )
        return 0;
    helper = instruction->integerTarget == XL6_IR_INTEGER_TARGET_UNSIGNED_64
        ? "xlir_u64_binary_checked" : "xlir_i64_binary_checked";
    operation = instruction->runtimeOp == XL6_IR_RUNTIME_INTEGER_DIV_CHECKED ? 0
        : instruction->runtimeOp == XL6_IR_RUNTIME_INTEGER_MOD_CHECKED ? 1
        : instruction->runtimeOp == XL6_IR_RUNTIME_INTEGER_SHL_CHECKED ? 2 : 3;
    xl6OutputPrintf(out, "    v%u = %s(v%u, v%u, %d, %d);\n",
        (unsigned int)instruction->result, helper,
        (unsigned int)module->operands[instruction->firstOperand],
        (unsigned int)module->operands[instruction->firstOperand + 1],
        operation, (int)instruction->integerTarget);
    return !xl6OutputHasFailed(out);
}

/* These are the stable public xvaluetype tags from XRT.  Mapping canonical IR
 * enums here keeps generated C mechanical: no source spelling or generic type
 * recovery reaches the emitter.  Zero is reserved as "unsupported" for these
 * two non-null tag partitions. */
