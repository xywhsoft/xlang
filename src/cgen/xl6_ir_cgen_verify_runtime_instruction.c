/* Closed runtime-operation validation, separate from storage/control ABI. */
static int xl6IrCgenVerifyRuntimeInstruction(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction,XL6_DiagList* diags)
{
    switch (instruction->kind) {
        case XL6_IR_INSTR_RUNTIME_CALL:
            if(xl6IrCOwnedRuntimeOpIs(instruction->runtimeOp)) {
                if(!xl6IrCOwnedInstructionCompatible(module,instruction))
                    return xl6IrCgenDiag(diags,module,instruction->origin,"C owner operation has no exact lifetime ABI");
                break;
            }
            if(instruction->runtimeOp==XL6_IR_RUNTIME_NATIVE_SCALAR_AS_CHECKED ||
                instruction->runtimeOp==XL6_IR_RUNTIME_NATIVE_SCALAR_CONVERT_CHECKED) {
                if(!xl6IrNativeScalarInstructionCompatible(module,instruction))
                    return xl6IrCgenDiag(diags,module,instruction->origin,"Native scalar operation has no full checked output contract");
                break;
            }
            if (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_FUTURE_CAST_CHECKED) {
                if (!xl6IrFutureCastInstructionCompatible(module,instruction))
                    return xl6IrCgenDiag(diags,module,instruction->origin,"Future cast has no complete identity/output contract");
                break;
            }
            if(instruction->runtimeOp==XL6_IR_RUNTIME_F32_AS_CHECKED) {
                if(!xl6IrFloat32AsInstructionCompatible(module,instruction))
                    return xl6IrCgenDiag(diags,module,instruction->origin,"F32 as has no complete lossless output contract");
                break;
            }
            if (instruction->runtimeOp==XL6_IR_RUNTIME_NATIVE_CONTAINER_PROJECT_CHECKED) {
                if (!xl6IrNativeContainerProjectionInstructionCompatible(module,instruction))
                    return xl6IrCgenDiag(diags,module,instruction->origin,"Native container projection has no declared owned schema boundary");
                break;
            }
            if(xl6IrSlotMapIteratorOpIs(instruction->runtimeOp)) {
                if(!xl6IrSlotMapIteratorInstructionCompatible(module,instruction))
                    return xl6IrCgenDiag(diags,module,instruction->origin,
                        "XLIR SlotMap cursor has no closed lane or owning key ABI");
                break;
            }
            if (xl6IrRuntimeNativeValueOpIs(instruction->runtimeOp)) {
                if (!xl6IrNativeValueInstructionPlan(module,instruction))
                    return xl6IrCgenDiag(diags,module,instruction->origin,
                        "XLIR native value operation has no closed owning ABI");
                break;
            }
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
                  ((instruction->runtimeOp == XL6_IR_RUNTIME_STRING_TO_F64_CHECKED ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_F64_TO_F32_EXPLICIT) &&
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
                    instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_ENUM_BIND ||
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
                   (instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN &&
                    !xl6IrArrayImportRuntimeInstructionCompatible(module,instruction)) ||
                   (instruction->runtimeOp==XL6_IR_RUNTIME_ARRAY_CREATE &&
                    !xl6IrArrayCreateRuntimeInstructionCompatible(module,instruction)) ||
                   (instruction->runtimeOp==XL6_IR_RUNTIME_LIST_CREATE &&
                    !xl6IrListCreateRuntimeInstructionCompatible(module,instruction)) ||
                   (instruction->runtimeOp==XL6_IR_RUNTIME_DICT_CREATE &&
                    !xl6IrDictCreateRuntimeInstructionCompatible(module,instruction)) ||
                   (instruction->runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN &&
                    !xl6IrListImportRuntimeInstructionCompatible(module,instruction)) ||
                   (instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_SET_NEW &&
                    !xl6IrSetCreateRuntimeInstructionCompatible(module,instruction)) ||
                   (instruction->runtimeOp==XL6_IR_RUNTIME_SET_IMPORT_CHAIN &&
                    !xl6IrSetImportRuntimeInstructionCompatible(module,instruction)) ||
                   (instruction->runtimeOp==XL6_IR_RUNTIME_DICT_IMPORT_CHAIN &&
                    !xl6IrDictImportRuntimeInstructionCompatible(module,instruction)) ||
                   (instruction->runtimeOp==XL6_IR_RUNTIME_CLASS_DEFAULT_INVOKE &&
                    !xl6IrClassDefaultInvocationFunction(module,instruction)) ||
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
                 ((xl6IrListTypedReadRuntimeOpIs(instruction->runtimeOp) ||
                   instruction->runtimeOp==XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE) &&
                  !xl6IrListIndexRuntimeInstructionCompatible(module,instruction)) ||
                 (instruction->runtimeOp==XL6_IR_RUNTIME_DICT_SET_VALUE_TAKE &&
                  !xl6IrDictIndexWriteRuntimeInstructionCompatible(module,instruction)) ||
                 (xl6IrDictDirectSlotRuntimeOpIs(instruction->runtimeOp) &&
                  !xl6IrDictDirectSlotRuntimeInstructionCompatible(module,instruction)) ||
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
                   (instruction->immediateText != NULL || instruction->operandCount !=
                        (instruction->collectionAbi == XL6_IR_COLLECTION_ARRAY ? 3u : 2u) ||
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
                  (xl6IrPersistenceInvocationRuntime(instruction->runtimeOp) != NULL &&
                   !xl6IrPersistenceInvocationInstructionAbi(module, instruction)) ||
                  ((instruction->runtimeOp == XL6_IR_RUNTIME_PUBLIC_DATA_PROJECT || instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_PROJECT ||
                    instruction->runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_PROJECT) &&
                   !xl6IrPersistenceProjectInstructionAbi(module, instruction)) ||
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
                     instruction->runtimeOp != XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_F64_TO_F32_EXPLICIT &&
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
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ENUM_BIND &&
                     instruction->runtimeOp !=
                         XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND &&
                     instruction->runtimeOp !=
                         XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT &&
                     instruction->runtimeOp != XL6_IR_RUNTIME_CLASS_DEFAULT_INVOKE &&
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
                   instruction->runtimeOp != XL6_IR_RUNTIME_STACK_ERASE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_RETAIN &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_BYTES_RETAIN &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_CLONE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_DEEP_CLONE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_PUBLIC_DATA_PROJECT && instruction->runtimeOp != XL6_IR_RUNTIME_PERSISTENCE_JSON_PROJECT &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_PERSISTENCE_XLON_PROJECT &&
                   instruction->runtimeOp !=
                       XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_ARRAY_CREATE &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_LIST_CREATE &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_DICT_CREATE &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_LIST_IMPORT_CHAIN &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_SET_IMPORT_CHAIN &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_DICT_IMPORT_CHAIN &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_COUNT &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE &&
                    instruction->runtimeOp !=
                        XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL &&
                    instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_ARRAY_NEW &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_ARRAY_READ_VALUE_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_ARRAY_READ_VALUE_OR_NULL_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_INDEX_READ_VALUE_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_NAMED_READ_LITERAL_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_NAMED_SET_LITERAL_TAKE &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_LIST_READ_VALUE_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_LIST_READ_VALUE_OR_NULL_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_LIST_READ_VALUE_REQUIRED_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_DICT_READ_VALUE_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_DICT_READ_VALUE_OR_NULL_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_DICT_READ_VALUE_REQUIRED_COPY &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_DICT_SET_VALUE_TAKE &&
                   !xl6IrDictDirectSlotRuntimeOpIs(instruction->runtimeOp) &&
                   !xl6IrListTypedReadRuntimeOpIs(instruction->runtimeOp) &&
                   instruction->runtimeOp != XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE &&
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
                   xl6IrPersistenceInvocationRuntime(instruction->runtimeOp) == NULL &&
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
    default: return 0;
    }
    return 1;
}
