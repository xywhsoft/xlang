/* Validate each reachable instruction against its closed C emission ABI. */
static int xl6IrCgenVerifyInstructions(
    const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t i;
    for ( i = 0; i < module->instructionCount; ++i ) {
        const XL6_IrInstruction* instruction = &module->instructions[i];
        const XL6_IrFunction* owner = xl6IrCgenInstructionOwner(module, i);
        if ( owner == NULL )
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR C emitter instruction has no function error ABI");
        if ( owner->reachability == XL6_IR_REACHABILITY_UNREACHABLE ) continue;
        if ( instruction->result != XL6_IR_VALUE_INVALID &&
             !xl6IrCgenIsControlCarrier(module, instruction) &&
             xl6IrCgenType(module, instruction->type) == NULL )
            return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter does not support a value ABI");
        switch ( instruction->kind ) {
        case XL6_IR_INSTR_CONST:
            if ( instruction->immediateText == NULL ) return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR constant has no canonical immediate payload");
            break;
        case XL6_IR_INSTR_NULL:
            if ( !xl6IrCgenNullInstructionAbi(module, instruction) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter null ABI is unresolved");
            break;
        case XL6_IR_INSTR_NULL_TEST:
            {
                const XL6_IrInstruction* tested = instruction->operandCount == 1
                    ? xl6IrCgenValueDefinition(
                        module, module->operands[instruction->firstOperand], NULL) : NULL;
                const XL6_IrType* testedType = tested != NULL
                    ? xl6IrTypeGet(&module->types, tested->type) : NULL;
                const XL6_IrType* resultType = xl6IrTypeGet(&module->types, instruction->type);
                if ( instruction->operandCount != 1 || testedType == NULL || resultType == NULL ||
                     resultType->cAbi != XL6_IR_C_ABI_BOOL ||
                     (testedType->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL &&
                      testedType->nullAbi != XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
                      !(testedType->nullAbi == XL6_IR_NULL_ABI_TAGGED_VALUE &&
                        testedType->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE)) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter null-test ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_NULLABLE_WRAP:
        case XL6_IR_INSTR_NULLABLE_UNWRAP:
            {
                const XL6_IrInstruction* operand = instruction->operandCount == 1
                    ? xl6IrCgenValueDefinition(
                        module, module->operands[instruction->firstOperand], NULL) : NULL;
                const XL6_IrType* nullableType = instruction->kind == XL6_IR_INSTR_NULLABLE_WRAP
                    ? xl6IrTypeGet(&module->types, instruction->type)
                    : operand != NULL ? xl6IrTypeGet(&module->types, operand->type) : NULL;
                const XL6_IrType* element = NULL;
                XL6_IrTypeId elementType = nullableType != NULL &&
                    nullableType->paramCount == 1 && nullableType->params != NULL
                        ? nullableType->params[0] : XL6_IR_TYPE_INVALID;
                int taggedAbi = nullableType != NULL &&
                    nullableType->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE &&
                    nullableType->nullAbi == XL6_IR_NULL_ABI_TAGGED_VALUE;
                int sentinelAbi;
                if ( elementType != XL6_IR_TYPE_INVALID )
                    element = xl6IrTypeGet(&module->types, elementType);
                sentinelAbi = nullableType != NULL && element != NULL &&
                    (nullableType->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL ||
                     nullableType->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL) &&
                    nullableType->nullAbi == element->nullAbi &&
                    nullableType->cAbi == element->cAbi &&
                    nullableType->ownershipAbi == element->ownershipAbi;
                if ( instruction->operandCount != 1 || operand == NULL ||
                     nullableType == NULL ||
                     nullableType->kind != XL6_IR_TYPE_NULLABLE ||
                     xl6IrCgenType(module,
                        instruction->kind == XL6_IR_INSTR_NULLABLE_WRAP
                            ? instruction->type : operand->type) == NULL ||
                      (instruction->kind == XL6_IR_INSTR_NULLABLE_WRAP &&
                        ((!taggedAbi && !sentinelAbi) || operand->type != elementType)) ||
                     (instruction->kind == XL6_IR_INSTR_NULLABLE_UNWRAP &&
                       ((!taggedAbi && !sentinelAbi) ||
                        instruction->type != elementType)) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter nullable payload ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_UNION_ZERO:
            {
                const XL6_IrType* type = xl6IrTypeGet(&module->types, instruction->type);
                if ( type == NULL || type->kind != XL6_IR_TYPE_UNION ||
                     type->cAbi != XL6_IR_C_ABI_UNION ||
                     instruction->result == XL6_IR_VALUE_INVALID ||
                     instruction->operandCount != 0 ||
                     instruction->elementIndex != SIZE_MAX )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter union zero ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_UNION_GET:
        case XL6_IR_INSTR_UNION_SET:
            {
                const XL6_IrUnionField* field =
                    instruction->elementIndex < module->unionFieldCount
                        ? &module->unionFields[instruction->elementIndex] : NULL;
                const XL6_IrInstruction* source = instruction->operandCount >= 1
                    ? xl6IrCgenValueDefinition(
                        module, module->operands[instruction->firstOperand], NULL) : NULL;
                const XL6_IrInstruction* value = instruction->operandCount == 2
                    ? xl6IrCgenValueDefinition(
                        module, module->operands[instruction->firstOperand + 1], NULL) : NULL;
                if ( field == NULL || source == NULL || source->type != field->ownerType ||
                     instruction->result == XL6_IR_VALUE_INVALID ||
                     (instruction->kind == XL6_IR_INSTR_UNION_GET
                        ? instruction->operandCount != 1 || instruction->type != field->type
                        : instruction->operandCount != 2 || value == NULL ||
                          value->type != field->type || instruction->type != field->ownerType) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter union projection/update ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_RUNTIME_CALL:
            if ( xl6IrCgenSharedRuntimeOpIs(instruction->runtimeOp) ) {
                if ( !xl6IrCgenSharedRuntimeAbi(module, instruction) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter shared runtime ABI is unresolved");
                break;
            }
            if ( ((instruction->runtimeOp == XL6_IR_RUNTIME_STRING_FROM_UTF8 &&
                   (instruction->immediateText != NULL ||
                    (instruction->immediateData == NULL && instruction->immediateSize != 0) ||
                    instruction->operandCount != 0 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_FROM_DATA &&
                   (instruction->immediateText != NULL ||
                    (instruction->immediateData == NULL && instruction->immediateSize != 0) ||
                    instruction->operandCount != 0 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_DUP &&
                  (instruction->immediateText == NULL || instruction->operandCount != 0 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_COPY &&
                   (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_NULLABLE_COPY &&
                   !xl6IrCgenRuntimeNullableStringCopyAbi(module, instruction)) ||
                  (xl6IrCgenPrimitiveSequenceRuntimeOpIs(
                       instruction->runtimeOp) &&
                   !xl6IrCgenPrimitiveSequenceRuntimeAbi(
                       module, instruction)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_TO_F64_CHECKED &&
                   !xl6IrCgenRuntimeCheckedFloatConvertAbi(module, instruction)) ||
                  ((instruction->runtimeOp == XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED) &&
                   !xl6IrCgenRuntimeCheckedTimeConvertAbi(module, instruction)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED &&
                   !xl6IrCgenRuntimeIntegerConvertAbi(module, instruction)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING &&
                   !xl6IrCgenRuntimeIntegerWrappingAbi(module, instruction)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_CONCAT &&
                 (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                  xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW &&
                  (instruction->immediateText != NULL || instruction->operandCount != 3 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                   ((instruction->runtimeOp == XL6_IR_RUNTIME_STRING_EQUAL ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL) &&
                    (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   ((instruction->runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_LT ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_LE ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_GT ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_GE) &&
                    (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_UTF8_COUNT &&
                    (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_UTF8_AT &&
                    (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_STRING_CONTAINS &&
                    (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_I64_RANGE_CONTAINS &&
                    (instruction->immediateText != NULL || instruction->operandCount != 4 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   ((instruction->runtimeOp == XL6_IR_RUNTIME_BOOL_TO_STRING ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_I64_TO_STRING ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_U64_TO_STRING ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_F64_TO_STRING ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_STRING) &&
                    (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_TIME_TO_STRING &&
                    !xl6IrCgenRuntimeTimeToStringAbi(module, instruction)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_TYPE_ID_TO_STRING &&
                    !xl6IrCgenRuntimeTypeIdToStringAbi(module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING &&
                    !xl6IrCgenRuntimeClassDerivedStringAbi(
                        module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL &&
                    !xl6IrCgenRuntimeClassDerivedEqualAbi(
                        module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND &&
                    !xl6IrCgenRuntimeClassIdentityBindAbi(
                        module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT &&
                    !xl6IrCgenRuntimeClassConstructionCommitAbi(
                        module, instruction)) ||
                  ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TYPE_BIND ||
                    instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED) &&
                   (instruction->immediateText != NULL ||
                   instruction->operandCount != 1 ||
                   instruction->runtimeIdentityType == XL6_IR_TYPE_INVALID ||
                   xl6IrCgenType(module, instruction->type) == NULL ||
                   xl6IrTypeGet(&module->types,
                       instruction->runtimeIdentityType) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TYPE_ID &&
                  (instruction->immediateText != NULL ||
                   instruction->operandCount != 1 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NULL &&
                  (instruction->immediateText == NULL || instruction->operandCount != 0 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BOOL &&
                  (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CHAR &&
                  (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_I64 &&
                  (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_U64 &&
                   (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNION_BOX &&
                   !xl6IrCgenRuntimeUnionBoxAbi(module, instruction)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNION_UNBOX &&
                   !xl6IrCgenRuntimeUnionUnboxAbi(module, instruction)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_F64 &&
                   (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR &&
                    !xl6IrCgenRuntimeForeignScalarBoxAbi(module, instruction)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ERROR_REF &&
                    !xl6IrCgenRuntimeErrorXValueRefAbi(module, instruction)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TIME &&
                    !xl6IrCgenRuntimeTimeBoxAbi(module, instruction)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_STRING_COPY &&
                   (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_FUTURE_REF &&
                  !xl6IrCgenRuntimeFutureXValueRefAbi(module, instruction)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NULLABLE_STRING_BOX &&
                  !xl6IrCgenRuntimeNullableStringBoxAbi(module, instruction)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE &&
                  (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                   instruction->targetSymbol == 0 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_BORROW &&
                   (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW &&
                   (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                   ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_RETAIN ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_BYTES_RETAIN) &&
                    !xl6IrCgenRuntimeManagedXValueRetainAbi(module, instruction)) ||
                   ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CLONE ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_DEEP_CLONE) &&
                    (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST &&
                    !xl6IrCgenRuntimeCollectionCheckedCastAbi(
                        module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND &&
                    !xl6IrCgenRuntimeCollectionCheckedRebindAbi(
                        module, instruction)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_COUNT &&
                   (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                   ((instruction->runtimeOp ==
                         XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE ||
                     instruction->runtimeOp ==
                         XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL) &&
                    !xl6IrCollectionCountRuntimeInstructionCompatible(module, instruction)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_NEW &&
                  (instruction->immediateText != NULL || instruction->operandCount != 0 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64 &&
                   (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp ==
                       XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL &&
                   !xl6IrCgenRuntimeSafeIntegerIndexReadAbi(
                       module, instruction, XL6_IR_COLLECTION_ARRAY)) ||
                 (xl6IrArrayScalarReadRuntimeOpIs(instruction->runtimeOp) &&
                  !xl6IrArrayScalarReadRuntimeInstructionCompatible(module, instruction)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY &&
                  !xl6IrArrayStringReadRuntimeInstructionCompatible(module,instruction)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW &&
                  (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW &&
                  (instruction->immediateText != NULL || instruction->operandCount != 3 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW &&
                  (instruction->immediateText != NULL || instruction->operandCount != 0 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                  ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64 ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_INTEGER ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64 ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_BOOL ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_F64 ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY) &&
                  (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp ==
                       XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL &&
                   !xl6IrCgenRuntimeSafeIntegerIndexReadAbi(
                       module, instruction, XL6_IR_COLLECTION_LIST)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW &&
                  (instruction->immediateText != NULL || instruction->operandCount != 3 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_NEW &&
                  (instruction->immediateText != NULL || instruction->operandCount != 0 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW &&
                  (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_HAS &&
                   (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE &&
                   (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                    instruction->collectionAbi < XL6_IR_COLLECTION_ARRAY ||
                    instruction->collectionAbi > XL6_IR_COLLECTION_DICT ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CONTAINS &&
                    (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE &&
                    (instruction->immediateText != NULL ||
                     instruction->operandCount != 2 ||
                     instruction->collectionAbi != XL6_IR_COLLECTION_NONE ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (xl6IrCollectionAlgebraRuntimeOpIs(
                        instruction->runtimeOp) &&
                    !xl6IrCgenRuntimeCollectionAlgebraAbi(
                        module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
                    (instruction->immediateText != NULL ||
                     instruction->operandCount == 0 ||
                     !xl6IrTypedCollectionOpOperandCountValid(
                         instruction->typedCollectionOp,
                         instruction->operandCount - 1) ||
                     !xl6IrTypedCollectionOpSupportsCollection(
                         instruction->typedCollectionOp,
                         instruction->collectionAbi) ||
                     xl6IrCgenTypedCollectionOpToken(
                         instruction->typedCollectionOp) == NULL ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp ==
                       XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD &&
                   (instruction->immediateText != NULL ||
                    instruction->operandCount == 0 ||
                    !xl6IrDynamicCollectionOpOperandCountValid(
                        instruction->dynamicCollectionOp,
                        instruction->operandCount - 1) ||
                    xl6IrCgenDynamicCollectionOpToken(
                        instruction->dynamicCollectionOp) == NULL ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_NEW ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_NEW) &&
                  (instruction->immediateText != NULL || instruction->operandCount != 0 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW) &&
                  (instruction->immediateText == NULL || instruction->operandCount != 2 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL &&
                  (instruction->immediateText == NULL || instruction->operandCount != 1 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING &&
                  (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY &&
                  (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW &&
                  (instruction->immediateText != NULL || instruction->operandCount != 3 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64 &&
                  (instruction->immediateText == NULL || instruction->operandCount != 1 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64 &&
                   (instruction->immediateText == NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64 ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME ||
                     instruction->runtimeOp ==
                         XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER ||
                     instruction->runtimeOp ==
                         XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW ||
                     instruction->runtimeOp ==
                         XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW ||
                     instruction->runtimeOp ==
                         XL6_IR_RUNTIME_XVALUE_OBJECT_TAKE_LITERAL_NATIVE_HANDLE) &&
                   (instruction->immediateText == NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (xl6IrCgenTypedDictLiteralRuntimeOpIs(
                       instruction->runtimeOp) &&
                   (instruction->immediateText == NULL ||
                    instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                   ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX ||
                    instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE) &&
                   !xl6IrCgenRuntimeTaggedNullableXValueAbi(module, instruction)) ||
                  (xl6IrCgenCheckedIntegerRuntimeOpIs(
                       instruction->runtimeOp) &&
                   !xl6IrCgenRuntimeCheckedIntegerArithmeticAbi(
                       module, instruction)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_I64_POW &&
                   !xl6IrCgenRuntimeI64PowAbi(module, instruction)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_TIME_APPROX &&
                   !xl6IrCgenRuntimeTimeApproxAbi(module, instruction)) ||
                  ((instruction->runtimeOp ==
                        XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY ||
                    instruction->runtimeOp ==
                        XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY) &&
                   !xl6IrCgenPersistenceStringifyAbi(module, instruction)) ||
                  ((instruction->runtimeOp ==
                        XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE ||
                    instruction->runtimeOp ==
                        XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE ||
                    instruction->runtimeOp ==
                        XL6_IR_RUNTIME_PERSISTENCE_JSON_READ ||
                    instruction->runtimeOp ==
                        XL6_IR_RUNTIME_PERSISTENCE_XLON_READ) &&
                   !xl6IrCgenPersistenceParseAbi(module, instruction)) ||
                  ((instruction->runtimeOp ==
                        XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITE ||
                    instruction->runtimeOp ==
                        XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITE) &&
                   !xl6IrCgenPersistenceWriteAbi(module, instruction)) ||
                 ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_ADD ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SUB ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MUL ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_DIV ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MOD ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_AND ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_OR ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_XOR ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHL ||
                   instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHR) &&
                  (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                   xl6IrCgenType(module, instruction->type) == NULL)) ||
                  ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_POS ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_NEG ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT) &&
                   (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  ((instruction->runtimeOp >= XL6_IR_RUNTIME_I64_APPROX &&
                    instruction->runtimeOp <= XL6_IR_RUNTIME_STRING_APPROX) &&
                   (instruction->immediateText != NULL || instruction->operandCount != 2 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TRUTHY &&
                   (instruction->immediateText != NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY &&
                   (instruction->immediateText == NULL || instruction->operandCount != 1 ||
                    xl6IrCgenType(module, instruction->type) == NULL)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE &&
                   !xl6IrCgenRuntimeObjectClosureFieldAbi(module, instruction, 1)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE &&
                   !xl6IrCgenRuntimeObjectClosureFieldAbi(module, instruction, 0)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE &&
                   !xl6IrCgenRuntimeCallableFromClosureAbi(module, instruction)) ||
                 (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NULLABLE_CLOSURE_BOX &&
                  !xl6IrCgenRuntimeNullableClosureBoxAbi(module, instruction)) ||
                  ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL ||
                    instruction->runtimeOp ==
                        XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL) &&
                   !xl6IrCgenRuntimeDynamicCallAbi(module, instruction)) ||
                  (xl6IrCgenThreadRuntimeOpIs(instruction->runtimeOp) &&
                   !xl6IrCgenRuntimeThreadAbi(module, instruction)) ||
                  (xl6IrCgenSyncRuntimeOpIs(instruction->runtimeOp) &&
                   !xl6IrCgenRuntimeSyncAbi(module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE &&
                    !xl6IrCgenRuntimeTaskSpawnAbi(module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE &&
                    !xl6IrCgenRuntimeFutureContinuationAbi(
                        module, instruction)) ||
                  (instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_CANCEL &&
                   !xl6IrCgenRuntimeFutureCancelAbi(module, instruction)) ||
                  (xl6IrCgenFutureMemberHelper(instruction->runtimeOp) != NULL &&
                   !xl6IrCgenRuntimeFutureMemberAbi(module, instruction)) ||
                   ((instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_ALL_XVALUES ||
                     instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_RACE) &&
                    !xl6IrCgenRuntimeFutureAggregateAbi(module, instruction)) ||
                   ((instruction->runtimeOp ==
                         XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY ||
                     instruction->runtimeOp ==
                         XL6_IR_RUNTIME_FUTURE_RACE_ARRAY) &&
                    !xl6IrCgenRuntimeFutureAggregateArrayAbi(
                        module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX &&
                    !xl6IrCgenRuntimeFutureWhenAnyIndexAbi(
                        module, instruction)) ||
                   (instruction->runtimeOp ==
                        XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY &&
                    !xl6IrCgenRuntimeFutureWhenAnyIndexArrayAbi(
                        module, instruction)) ||
                   (xl6IrCgenXValueIteratorRuntimeOpIs(
                        instruction->runtimeOp) &&
                    (instruction->runtimeOp !=
                         XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED ||
                     xl6IrCgenRuntimeCheckedIteratorCreateAbi(
                         module, instruction)) &&
                     (instruction->immediateText != NULL ||
                     instruction->operandCount != 1 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
              (xl6IrCgenXValueProjectionRuntimeOpIs(
                        instruction->runtimeOp) &&
                    (instruction->immediateText != NULL ||
                     instruction->operandCount != 1 ||
                     xl6IrCgenType(module, instruction->type) == NULL)) ||
                   (instruction->runtimeOp != XL6_IR_RUNTIME_STRING_FROM_UTF8 &&
                      instruction->runtimeOp != XL6_IR_RUNTIME_BYTES_FROM_DATA &&
                     !xl6IrCgenPrimitiveSequenceRuntimeOpIs(
                         instruction->runtimeOp) &&
                      instruction->runtimeOp != XL6_IR_RUNTIME_STRING_DUP &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_STRING_COPY &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_STRING_NULLABLE_COPY &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_STRING_TO_F64_CHECKED &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_STRING_CONCAT &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_STRING_EQUAL &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_NULLABLE_SCALAR_EQUAL &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_STRING_ORDER_LT &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_STRING_ORDER_LE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_STRING_ORDER_GT &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_STRING_ORDER_GE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_STRING_UTF8_COUNT &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_STRING_UTF8_AT &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_STRING_CONTAINS &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_I64_RANGE_CONTAINS &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_BOOL_TO_STRING &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_I64_TO_STRING &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_U64_TO_STRING &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_F64_TO_STRING &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_TIME_TO_STRING &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_TYPE_ID_TO_STRING &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TO_STRING &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TYPE_BIND &&
                     instruction->runtimeOp !=
                         XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND &&
                     instruction->runtimeOp !=
                         XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TYPE_ID &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_NULL &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BOOL &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CHAR &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_I64 &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_U64 &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_UNION_BOX &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_UNION_UNBOX &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_F64 &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ERROR_REF &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TIME &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_STRING_COPY &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_FUTURE_REF &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_BORROW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_RETAIN &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_BYTES_RETAIN &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CLONE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_DEEP_CLONE &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_COUNT &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_NEW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_ARRAY_READ_VALUE_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_ARRAY_READ_VALUE_OR_NULL_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_INDEX_READ_VALUE_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64 &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_U64 &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_F64 &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64 &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_INTEGER &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64 &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_BOOL &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_F64 &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_SET_NEW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_SET_HAS &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CONTAINS &&
                   instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE &&
                   !xl6IrCollectionAlgebraRuntimeOpIs(
                        instruction->runtimeOp) &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_NEW &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CLASS_NEW &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING &&
                  instruction->runtimeOp !=
                      XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW &&
                  instruction->runtimeOp !=
                      XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL &&
                  instruction->runtimeOp !=
                      XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_STRING &&
                  instruction->runtimeOp !=
                      XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_STRING_NEW &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64 &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64 &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64 &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW &&
                   instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW &&
                   instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_OBJECT_TAKE_LITERAL_NATIVE_HANDLE &&
                    !xl6IrCgenTypedDictLiteralRuntimeOpIs(
                         instruction->runtimeOp) &&
                    !xl6IrCgenCheckedIntegerRuntimeOpIs(instruction->runtimeOp) &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_I64_POW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_TIME_APPROX &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITE &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_PERSISTENCE_JSON_READ &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITE &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_PERSISTENCE_XLON_READ &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_ADD &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_SUB &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_MUL &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_DIV &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_MOD &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_BIT_AND &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_BIT_OR &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_BIT_XOR &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_SHL &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_BINARY_SHR &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_UNARY_POS &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_UNARY_NEG &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT &&
                   (instruction->runtimeOp < XL6_IR_RUNTIME_I64_APPROX ||
                    instruction->runtimeOp > XL6_IR_RUNTIME_STRING_APPROX) &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TRUTHY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_FUTURE_REF &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_NULLABLE_STRING_BOX &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_NULLABLE_CLOSURE_BOX &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CLASS_IS &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CLASS_AS &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST &&
                  instruction->runtimeOp !=
                      XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED &&
                  instruction->runtimeOp !=
                      XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE &&
                  instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL &&
                  instruction->runtimeOp !=
                      XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL &&
                      !xl6IrCgenThreadRuntimeOpIs(instruction->runtimeOp) &&
                      !xl6IrCgenChannelRuntimeOpIs(instruction->runtimeOp) &&
                      !xl6IrCgenSpecialQueueRuntimeOpIs(
                          instruction->runtimeOp) &&
                      !xl6IrCgenStackRuntimeOpIs(
                           instruction->runtimeOp) &&
                      !xl6IrCgenAvlRuntimeOpIs(
                           instruction->runtimeOp) &&
                      !xl6IrCgenSyncRuntimeOpIs(instruction->runtimeOp) &&
                      instruction->runtimeOp !=
                          XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE &&
                      instruction->runtimeOp !=
                          XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_CANCEL &&
                      xl6IrCgenFutureMemberHelper(instruction->runtimeOp) == NULL &&
                      instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
                      instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_RACE &&
                      instruction->runtimeOp !=
                          XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY &&
                      instruction->runtimeOp !=
                          XL6_IR_RUNTIME_FUTURE_RACE_ARRAY &&
                      instruction->runtimeOp !=
                          XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX &&
                      instruction->runtimeOp !=
                          XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY &&
                    !xl6IrCgenXValueIteratorRuntimeOpIs(
                        instruction->runtimeOp) &&
              !xl6IrCgenXValueProjectionRuntimeOpIs(
                        instruction->runtimeOp))) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter runtime operation ABI is unresolved");
            break;
        case XL6_IR_INSTR_RELEASE:
        case XL6_IR_INSTR_VALUE_RELEASE:
            if ( xl6IrCgenType(module, instruction->type) == NULL ||
                 (instruction->kind == XL6_IR_INSTR_RELEASE &&
                  xl6IrStorageGet(module, instruction->targetSymbol) == NULL) ||
                 !xl6IrTypeHasStorageReleaseAbi(
                     module, instruction->type) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter ownership runtime ABI is not implemented");
            break;
        case XL6_IR_INSTR_MOVE:
        case XL6_IR_INSTR_STORAGE_TAKE:
            if ( instruction->operandCount != (instruction->kind == XL6_IR_INSTR_STORAGE_TAKE ? 0u : 1u) ||
                 xl6IrCgenType(module, instruction->type) == NULL ||
                 xl6IrStorageGet(module, instruction->targetSymbol) == NULL )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter move ownership ABI is not implemented");
            break;
        case XL6_IR_INSTR_LOAD:
            if ( xl6IrStorageGet(module, instruction->targetSymbol) == NULL ) return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR load has no explicit storage ABI");
            break;
        case XL6_IR_INSTR_STORE:
        case XL6_IR_INSTR_PRODUCT_STORE_MOVE:
            if ( instruction->operandCount != 1 || xl6IrStorageGet(module, instruction->targetSymbol) == NULL ) return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR store has no deterministic storage ABI");
            break;
        case XL6_IR_INSTR_STORAGE_ADDRESS:
            {
                const XL6_IrStorage* storage = xl6IrStorageGet(
                    module, instruction->targetSymbol);
                const XL6_IrType* reference = xl6IrTypeGet(
                    &module->types, instruction->type);
                if ( storage == NULL || instruction->operandCount != 0 ||
                     instruction->result == XL6_IR_VALUE_INVALID ||
                     reference == NULL || reference->kind != XL6_IR_TYPE_STORAGE_REF ||
                     reference->cAbi != XL6_IR_C_ABI_STORAGE_REF_PTR ||
                     reference->paramCount != 1 || reference->params == NULL ||
                     reference->params[0] != storage->type )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter storage-reference ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_CELL_NEW:
        case XL6_IR_INSTR_CELL_INIT:
        case XL6_IR_INSTR_CELL_LOAD:
        case XL6_IR_INSTR_CELL_STORE:
        case XL6_IR_INSTR_CELL_EXCHANGE:
        case XL6_IR_INSTR_CELL_RETAIN:
        case XL6_IR_INSTR_CELL_RELEASE:
            {
                const XL6_IrStorage* storage = xl6IrStorageGet(module, instruction->targetSymbol);
                const XL6_IrType* cell = storage != NULL
                    ? xl6IrTypeGet(&module->types, storage->cellType) : NULL;
                const XL6_IrType* element = cell != NULL && cell->paramCount == 1
                    ? xl6IrTypeGet(&module->types, cell->params[0]) : NULL;
                if ( storage == NULL || cell == NULL || cell->kind != XL6_IR_TYPE_CELL ||
                     xl6IrCgenCellKind(element) == NULL )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter cell element ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_CLOSURE_MAKE:
            {
                const XL6_IrType* type = xl6IrTypeGet(&module->types, instruction->type);
                const XL6_IrClosureTarget* closure = NULL;
                size_t closureIndex;
                for ( closureIndex = 0; closureIndex < module->closureTargetCount; ++closureIndex )
                    if ( module->closureTargets[closureIndex].targetSymbol == instruction->targetSymbol ) {
                        closure = &module->closureTargets[closureIndex]; break;
                    }
                if ( closure == NULL || instruction->operandCount != closure->captureCount ||
                     instruction->result == XL6_IR_VALUE_INVALID ||
                     type == NULL || type->cAbi != XL6_IR_C_ABI_CLOSURE_PTR ||
                     closure->signatureType != instruction->type )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter closure construction ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_CLOSURE_CALL:
            {
                const XL6_IrType* signature = xl6IrTypeGet(&module->types, instruction->calleeType);
                if ( signature == NULL || signature->kind != XL6_IR_TYPE_FUNCTION ||
                     signature->cAbi != XL6_IR_C_ABI_CLOSURE_PTR || instruction->targetSymbol != 0 ||
                     (signature->paramCount != 0 &&
                      (signature->parameterModes == NULL ||
                       signature->parameterAbis == NULL)) ||
                     instruction->operandCount != signature->paramCount + 1 ||
                     instruction->result == XL6_IR_VALUE_INVALID ||
                     (signature->callableResultType == XL6_IR_TYPE_INVALID &&
                      instruction->type != XL6_IR_TYPE_INVALID) ||
                     (signature->callableResultType != XL6_IR_TYPE_INVALID &&
                      instruction->type != signature->callableResultType) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter closure invoke ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_SCALAR_CONVERT:
            if ( instruction->operandCount != 1 || instruction->result == XL6_IR_VALUE_INVALID ||
                 xl6IrCgenType(module, instruction->type) == NULL ||
                 instruction->scalarConversionAbi <=
                     XL6_IR_SCALAR_CONVERSION_INVALID ||
                 instruction->scalarConversionAbi >
                     XL6_IR_SCALAR_CONVERSION_OPERATOR_I64_TO_U64 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                      "XLIR C emitter scalar conversion ABI is unresolved");
            break;
        case XL6_IR_INSTR_NATIVE_HANDLE_BIND:
            {
                const XL6_IrInstruction* source = instruction->operandCount == 1
                    ? xl6IrCgenValueDefinition(
                        module, module->operands[instruction->firstOperand], NULL) : NULL;
                const XL6_IrType* sourceType = source != NULL
                    ? xl6IrTypeGet(&module->types, source->type) : NULL;
                const XL6_IrType* resultType =
                    xl6IrTypeGet(&module->types, instruction->type);
                const XL6_IrNativeHandleBind* bind =
                    xl6IrNativeHandleBindGet(module, instruction->origin);
                if ( instruction->operandCount != 1 ||
                     instruction->result == XL6_IR_VALUE_INVALID ||
                     sourceType == NULL || sourceType->cAbi != XL6_IR_C_ABI_RAW_POINTER ||
                     resultType == NULL || resultType->cAbi != XL6_IR_C_ABI_NATIVE_HANDLE ||
                     bind == NULL || bind->sourceType != source->type ||
                     bind->targetType != instruction->type )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter native handle bind ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_NATIVE_HANDLE_PROJECT:
            {
                const XL6_IrInstruction* source = instruction->operandCount == 1
                    ? xl6IrCgenValueDefinition(
                        module, module->operands[instruction->firstOperand], NULL) : NULL;
                const XL6_IrType* sourceType = source != NULL
                    ? xl6IrTypeGet(&module->types, source->type) : NULL;
                const XL6_IrType* resultType =
                    xl6IrTypeGet(&module->types, instruction->type);
                if ( instruction->operandCount != 1 ||
                     instruction->result == XL6_IR_VALUE_INVALID ||
                     sourceType == NULL || sourceType->cAbi != XL6_IR_C_ABI_NATIVE_HANDLE ||
                     resultType == NULL || resultType->cAbi != XL6_IR_C_ABI_RAW_POINTER ||
                     resultType->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter native handle projection ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_PRODUCT_MAKE:
        case XL6_IR_INSTR_PRODUCT_GET:
        case XL6_IR_INSTR_PRODUCT_TAKE:
        case XL6_IR_INSTR_PRODUCT_DROP:
            if (!xl6IrCgenProductInstructionAbi(module, instruction))
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR product instruction has no exact structural/ownership ABI");
            break;
        case XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK:
            /* Already verified, including source, target and both owner edges. */
            break;
        case XL6_IR_INSTR_CALL:
            {
                const XL6_IrCallTarget* callee = xl6IrCallTargetGet(module, instruction->targetSymbol);
                if ( callee == NULL ||
                     callee->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC ||
                     callee->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR ||
                     (callee->variadicAbi == XL6_IR_CALL_VARIADIC_C
                        ? instruction->operandCount < callee->parameterCount
                        : callee->parameterCount != instruction->operandCount) ||
                     !xl6IrCgenCallResultAbiIsExact(
                         module, callee, instruction) )
                    return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter call target or argument ABI is unresolved");
                if ( callee->errorPhysicalAbi ==
                         XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER &&
                     xl6IrCgenErrorResultType(module, callee->returnType) == NULL )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter may-raise call has no result carrier ABI");
            }
            break;
        case XL6_IR_INSTR_VIRTUAL_CALL:
            if (!xl6IrCgenVirtualCallAbi(module, instruction))
                return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter virtual dispatch ABI is unresolved");
            break;
        case XL6_IR_INSTR_PROTOCOL_CALL:
            if ( !xl6IrCgenProtocolCallAbi(module, instruction) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter protocol dispatch ABI is unresolved");
            break;
        case XL6_IR_INSTR_ASYNC_START:
            {
                const XL6_IrCallTarget* callee = xl6IrCallTargetGet(module, instruction->targetSymbol);
                if ( callee == NULL || callee->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC ||
                     (callee->asyncDispatch != XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL &&
                      callee->asyncDispatch != XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE) ||
                     callee->parameterCount != instruction->operandCount ||
                     instruction->result == XL6_IR_VALUE_INVALID || instruction->type != callee->returnType )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter async launch ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_GENERATOR_START:
            {
                const XL6_IrCallTarget* callee =
                    xl6IrCallTargetGet(module, instruction->targetSymbol);
                if ( callee == NULL ||
                     callee->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR ||
                     xl6IrCgenGeneratorMachine(module, callee) == NULL ||
                     callee->parameterCount != instruction->operandCount ||
                     instruction->result == XL6_IR_VALUE_INVALID ||
                     instruction->type != callee->returnType )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter generator launch ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_GENERATOR_NEXT:
            {
                const XL6_IrInstruction* source = instruction->operandCount == 1
                    ? xl6IrCgenValueDefinition(module,
                        module->operands[instruction->firstOperand], NULL)
                    : NULL;
                const XL6_IrType* generator = source != NULL
                    ? xl6IrTypeGet(&module->types, source->type) : NULL;
                const XL6_IrType* result =
                    xl6IrTypeGet(&module->types, instruction->type);
                if ( source == NULL || generator == NULL || result == NULL ||
                     generator->cAbi != XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR ||
                     result->cAbi != XL6_IR_C_ABI_BOOL ||
                     instruction->operandCount != 1 ||
                     instruction->result == XL6_IR_VALUE_INVALID )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter generator next ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_GENERATOR_ITEM_GET:
        case XL6_IR_INSTR_GENERATOR_ITEM_TAKE:
            {
                const XL6_IrInstruction* source = instruction->operandCount == 1
                    ? xl6IrCgenValueDefinition(module,
                        module->operands[instruction->firstOperand], NULL)
                    : NULL;
                const XL6_IrType* generator = source != NULL
                    ? xl6IrTypeGet(&module->types, source->type) : NULL;
                if ( source == NULL || generator == NULL ||
                     generator->cAbi != XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR ||
                     generator->paramCount != 1 || generator->params == NULL ||
                     instruction->type != generator->params[0] ||
                     instruction->result == XL6_IR_VALUE_INVALID )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter generator item ABI is unresolved");
            }
            break;
        case XL6_IR_INSTR_BINARY:
            if ( xl6IrCgenOperator(instruction->opcode) == NULL || instruction->operandCount != 2 ) return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter has no deterministic binary operator rule");
            break;
        case XL6_IR_INSTR_UNARY:
            if ( xl6IrCgenOperator(instruction->opcode) == NULL || instruction->operandCount != 1 ) return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter has no deterministic unary operator rule");
            break;
        case XL6_IR_INSTR_VALUE:
            if ( instruction->operandCount != 1 ||
                 ((instruction->result == XL6_IR_VALUE_INVALID) ==
                  (instruction->targetValue == XL6_IR_VALUE_INVALID)) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR value copy has no deterministic definition or merge target");
            break;
        case XL6_IR_INSTR_DISCARD:
            if ( !xl6IrCgenDiscardAbi(module, instruction) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR discard has no deterministic non-owning observation ABI");
            break;
        case XL6_IR_INSTR_BRANCH:
        case XL6_IR_INSTR_JUMP:
        case XL6_IR_INSTR_RETURN:
        case XL6_IR_INSTR_YIELD:
        case XL6_IR_INSTR_GENERATOR_CLOSE_TEST:
            break;
        case XL6_IR_INSTR_TRAP_IF_FALSE:
            if ( instruction->result != XL6_IR_VALUE_INVALID || instruction->type != XL6_IR_TYPE_INVALID ||
                 instruction->operandCount != 1 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter runtime trap ABI is unresolved");
            break;
        case XL6_IR_INSTR_THROW:
            if ( owner->kind != XL6_IR_FUNCTION_MODULE_INIT &&
                 owner->kind != XL6_IR_FUNCTION_GENERATOR_WORKER &&
                 (owner->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE ||
                  xl6IrCgenErrorResultType(module, owner->returnType) == NULL) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter runtime exception ABI is not implemented");
            break;
        case XL6_IR_INSTR_AWAIT:
            {
            const XL6_IrType* type = xl6IrTypeGet(&module->types, instruction->type);
            int matches = instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID
                ? instruction->type == XL6_IR_TYPE_INVALID
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_BOOL
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_BOOL
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_CHAR
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_CHAR_U32
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_I64
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_I64
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_U64
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_U64
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_TYPE_ID
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_TYPE_ID
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_F64
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_F64
                : instruction->asyncResultAbi ==
                      XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_RAW_POINTER &&
                  type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_OWNED_STRING
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_MANAGED_STRING
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_XVALUE
                ? type != NULL &&
                  (type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
                   type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES)
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_ERROR
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_FUTURE
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_TAGGED
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE : 0;
            if ( instruction->operandCount != 1 || instruction->result == XL6_IR_VALUE_INVALID || !matches )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter await result ABI is not implemented");
            break;
            }
        case XL6_IR_INSTR_ERROR_TEST:
            if ( instruction->operandCount != 1 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter error-result test ABI is not implemented");
            break;
        case XL6_IR_INSTR_ERROR_RAISE:
        case XL6_IR_INSTR_CLEANUP_BEGIN:
        case XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN:
        case XL6_IR_INSTR_CLEANUP_CAPTURE:
        case XL6_IR_INSTR_CLEANUP_FAIL:
        case XL6_IR_INSTR_CLEANUP_END:
        case XL6_IR_INSTR_ERROR_UNWIND_BEGIN:
        case XL6_IR_INSTR_ERROR_UNWIND_END:
        case XL6_IR_INSTR_ERROR_TAKE:
        case XL6_IR_INSTR_ERROR_SET_TAKE:
        case XL6_IR_INSTR_ERROR_CLEAR:
            if (!xl6IrCgenErrorStateInstructionAbi(module, instruction))
                return xl6IrCgenDiag(diags, module, instruction->origin,
                    "XLIR error-state ownership ABI is unresolved");
            break;
        case XL6_IR_INSTR_RETAIN:
            if ( !xl6IrCgenRetainInstructionAbi(
                     module, instruction) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter retain ABI is unresolved");
            break;
        case XL6_IR_INSTR_DROP:
            return xl6IrCgenDiag(diags, module, instruction->origin,
                                 "XLIR C emitter ownership runtime ABI is not implemented");
        default:
            return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter has no rule for this lowered instruction");
        }
    }
    return 1;
}
