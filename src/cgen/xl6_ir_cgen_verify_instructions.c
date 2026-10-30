#include "src/cgen/xl6_ir_cgen_verify_runtime_instruction.c"
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
                        (testedType->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE || xl6IrNullableNativeValuePlan(&module->types,testedType)!=NULL || xl6IrTypeIsCOwned(testedType)))) )
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
                    ((nullableType->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE &&
                    nullableType->nullAbi == XL6_IR_NULL_ABI_TAGGED_VALUE) || xl6IrNullableNativeValuePlan(&module->types,nullableType)!=NULL || xl6IrTypeIsCOwned(nullableType));
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
            if (!xl6IrCgenVerifyRuntimeInstruction(module,instruction,diags)) return 0;
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
                     (signature->callableErasureAbi == XL6_IR_CALLABLE_ERASURE_C_VARIADIC
                        ? instruction->operandCount < signature->paramCount
                        : instruction->operandCount != signature->paramCount + 1) ||
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
                     (callee->asyncDispatch != XL6_IR_ASYNC_DISPATCH_FIXED_READY_QUEUE &&
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
        case XL6_IR_INSTR_GENERATOR_PREPARE_YIELD:
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
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_F32
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_F32
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_TIME
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_TIME_I64
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
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE
                ? xl6IrNativeValuePlanForType(type) != NULL
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_TAGGED
                ? type != NULL && type->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_C_SCALAR
                ? xl6IrAsyncResultAbiForType(type)==XL6_IR_ASYNC_RESULT_COPY_C_SCALAR
                : instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_UNION
                ? xl6IrAsyncResultAbiForType(type)==XL6_IR_ASYNC_RESULT_COPY_UNION &&
                    xl6IrUnionLayoutGet(module,instruction->type)!=NULL : 0;
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
