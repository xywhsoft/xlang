/* Dispatch one verified synchronous XLIR instruction.  RuntimeOp families are
 * delegated to closed domain emitters rather than recovered from source AST. */
static int xl6IrCgenEmitInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrFunction* function, const XL6_IrInstruction* instruction,
    const XL6_IrClosureTarget* closure,
    size_t instructionIndex, XL6_DiagList* diags)
{
    switch ( instruction->kind ) {
            case XL6_IR_INSTR_CONST:
                if (xl6IrCgenResultStorageOmitted(module, instruction)) break;
                if ( instruction->immediateText == NULL ) return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR constant has no canonical immediate payload");
                xl6OutputPrintf(out, "    v%u = %s;\n", (unsigned int)instruction->result, instruction->immediateText);
                break;
            case XL6_IR_INSTR_NULL:
                if ( !xl6IrCgenEmitNullInstruction(
                        out, module, instruction, diags) ) return 0;
                break;
            case XL6_IR_INSTR_NULL_TEST:
            case XL6_IR_INSTR_NULLABLE_WRAP:
            case XL6_IR_INSTR_NULLABLE_UNWRAP:
                if ( !xl6IrCgenEmitNullableInstruction(
                        out, module, instruction, diags) ) return 0;
                break;
            case XL6_IR_INSTR_UNION_ZERO:
                xl6OutputPrintf(out, "    v%u = (%s){0};\n",
                    (unsigned int)instruction->result,
                    xl6IrCgenType(module, instruction->type));
                break;
            case XL6_IR_INSTR_UNION_GET: {
                const XL6_IrUnionField* field =
                    instruction->elementIndex < module->unionFieldCount
                        ? &module->unionFields[instruction->elementIndex] : NULL;
                if ( field == NULL || field->cName[0] == '\0' )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR union read has no exact C member ABI");
                xl6OutputPrintf(out, "    v%u = v%u.%s;\n",
                    (unsigned int)instruction->result,
                    (unsigned int)module->operands[instruction->firstOperand],
                    field->cName);
                break;
            }
            case XL6_IR_INSTR_UNION_SET: {
                const XL6_IrUnionField* field =
                    instruction->elementIndex < module->unionFieldCount
                        ? &module->unionFields[instruction->elementIndex] : NULL;
                if ( field == NULL || field->cName[0] == '\0' )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR union write has no exact C member ABI");
                xl6OutputPrintf(out, "    v%u = v%u;\n    v%u.%s = v%u;\n",
                    (unsigned int)instruction->result,
                    (unsigned int)module->operands[instruction->firstOperand],
                    (unsigned int)instruction->result,
                    field->cName,
                    (unsigned int)module->operands[instruction->firstOperand + 1]);
                break;
            }
            case XL6_IR_INSTR_LOAD:
                {
                    const XL6_IrStorage* storage = xl6IrStorageGet(module, instruction->targetSymbol);
                    XL6_IrParameterAbi parameterAbi =
                        xl6IrCgenStorageParameterAbi(module, storage, NULL);
                    size_t captureIndex;
                    if ( storage == NULL )
                    return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR load has no explicit storage ABI");
                    if ( xl6IrCgenResultStorageOmitted(module, instruction) )
                        break;
                    if ( storage->kind == XL6_IR_STORAGE_CAPTURE ) {
                        const XL6_IrClosureCapture* capture = xl6IrCgenCaptureGet(module, storage->symbol, &captureIndex);
                        if ( closure == NULL || capture == NULL || capture->type != storage->type )
                            return xl6IrCgenDiag(diags, module, instruction->origin,
                                                 "XLIR C emitter capture load ABI is unresolved");
                        xl6OutputPrintf(out, "    v%u = ((xlir_closure_env_%u*)environment)->c%lu;\n",
                                        (unsigned int)instruction->result,
                                        (unsigned int)closure->targetSymbol, (unsigned long)captureIndex);
                    } else {
                        xl6OutputPrintf(out, "    v%u = ", (unsigned int)instruction->result);
                        if ( xl6IrCgenParameterAbiIsIndirect(parameterAbi) )
                            xl6OutputPuts("*", out);
                        xl6IrCgenPutStorageName(out, module, instruction->targetSymbol);
                        xl6OutputPuts(";\n", out);
                    }
                }
                break;
            case XL6_IR_INSTR_STORE:
            case XL6_IR_INSTR_PRODUCT_STORE_MOVE:
                if (!xl6IrCgenEmitStorageCommit(out,module,instruction,SIZE_MAX,SIZE_MAX,diags)) return 0;
                break;
            case XL6_IR_INSTR_STORAGE_ADDRESS:
                {
                    const XL6_IrStorage* storage = xl6IrStorageGet(
                        module, instruction->targetSymbol);
                    if ( storage == NULL )
                        return xl6IrCgenDiag(diags, module, instruction->origin,
                            "XLIR storage address has no explicit storage ABI");
                    xl6OutputPrintf(out, "    v%u = (void*)",
                        (unsigned int)instruction->result);
                    if ( !xl6IrCgenParameterAbiIsIndirect(
                             xl6IrCgenStorageParameterAbi(
                                 module, storage, NULL)) )
                        xl6OutputPuts("&", out);
                    xl6IrCgenPutStorageName(
                        out, module, instruction->targetSymbol);
                    xl6OutputPuts(";\n", out);
                }
                break;
            case XL6_IR_INSTR_CELL_NEW:
            case XL6_IR_INSTR_CELL_INIT:
            case XL6_IR_INSTR_CELL_LOAD:
            case XL6_IR_INSTR_CELL_STORE:
            case XL6_IR_INSTR_CELL_EXCHANGE:
            case XL6_IR_INSTR_CELL_RETAIN:
            case XL6_IR_INSTR_CELL_RELEASE:
                if (!xl6IrCgenEmitCellInstruction(out, module, instruction, closure,
                        SIZE_MAX, SIZE_MAX, diags)) return 0;
                break;
            case XL6_IR_INSTR_CLOSURE_MAKE:
                if ( !xl6IrCgenEmitClosureMakeInstruction(
                        out, module, instruction, instructionIndex, diags) ) return 0;
                break;
            case XL6_IR_INSTR_CLOSURE_CALL:
                if ( !xl6IrCgenEmitClosureCallInstruction(
                        out, module, instruction, instructionIndex, diags) ) return 0;
                break;
            case XL6_IR_INSTR_SCALAR_CONVERT:
                {
                    const char* resultType = xl6IrCgenType(module, instruction->type);
                    if ( resultType == NULL || instruction->result == XL6_IR_VALUE_INVALID ||
                         instruction->operandCount != 1 ||
                         instruction->scalarConversionAbi <=
                             XL6_IR_SCALAR_CONVERSION_INVALID ||
                         instruction->scalarConversionAbi >
                             XL6_IR_SCALAR_CONVERSION_OPERATOR_I64_TO_U64 )
                        return xl6IrCgenDiag(diags, module, instruction->origin,
                                             "XLIR C emitter scalar conversion ABI is unresolved");
                    xl6OutputPrintf(out, "    v%u = (%s)v%u;\n",
                                    (unsigned int)instruction->result, resultType,
                                    (unsigned int)module->operands[instruction->firstOperand]);
                }
                break;
            case XL6_IR_INSTR_NATIVE_HANDLE_BIND:
                if ( instruction->result == XL6_IR_VALUE_INVALID ||
                     instruction->operandCount != 1 ||
                     xl6IrNativeHandleBindGet(module, instruction->origin) == NULL )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter native handle bind ABI is unresolved");
                xl6OutputPrintf(out, "    v%u = (void*)v%u;\n",
                                (unsigned int)instruction->result,
                                (unsigned int)module->operands[instruction->firstOperand]);
                break;
            case XL6_IR_INSTR_NATIVE_HANDLE_PROJECT:
                if ( instruction->result == XL6_IR_VALUE_INVALID ||
                     instruction->operandCount != 1 )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR C emitter native handle projection ABI is unresolved");
                xl6OutputPrintf(out, "    v%u = (void*)v%u;\n",
                                (unsigned int)instruction->result,
                                (unsigned int)module->operands[instruction->firstOperand]);
                break;
            case XL6_IR_INSTR_PRODUCT_MAKE:
            case XL6_IR_INSTR_PRODUCT_GET:
            case XL6_IR_INSTR_PRODUCT_TAKE:
            case XL6_IR_INSTR_PRODUCT_DROP:
                if (!xl6IrCgenEmitProductInstruction(out, module, instruction, diags)) return 0;
                break;
            case XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK:
                if (!xl6IrCgenEmitForeignResultRollback(out, module, instruction)) return 0;
                break;
            case XL6_IR_INSTR_CALL:
                if ( !xl6IrCgenEmitDirectCall(
                        out, module, instruction, instructionIndex, diags,
                        "XLIR C emitter call target or argument ABI is unresolved") )
                    return 0;
                break;
            case XL6_IR_INSTR_VIRTUAL_CALL:
                if (!xl6IrCgenEmitDispatchCallInstruction(out, module, instruction, diags)) return 0;
                break;
            case XL6_IR_INSTR_PROTOCOL_CALL:
                if ( !xl6IrCgenEmitDispatchCallInstruction(
                        out, module, instruction, diags) ) return 0;
                break;
            case XL6_IR_INSTR_ASYNC_START:
            case XL6_IR_INSTR_GENERATOR_START:
                if (!xl6IrCgenEmitLaunchCall(out, module, instruction, diags)) return 0;
                break;
            case XL6_IR_INSTR_GENERATOR_NEXT:
            case XL6_IR_INSTR_GENERATOR_ITEM_GET:
            case XL6_IR_INSTR_GENERATOR_ITEM_TAKE:
                if (!xl6IrCgenEmitGeneratorConsumerInstruction(out, module, instruction)) return 0;
                break;
            case XL6_IR_INSTR_BINARY:
                {
                    const char* op = xl6IrCgenOperator(instruction->opcode);
                    if ( op == NULL || instruction->operandCount != 2 )
                    return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter has no deterministic binary operator rule");
                xl6OutputPrintf(out, "    v%u = v%u %s v%u;\n", (unsigned int)instruction->result,
                                (unsigned int)module->operands[instruction->firstOperand], op,
                                (unsigned int)module->operands[instruction->firstOperand + 1]);
                }
                break;
            case XL6_IR_INSTR_UNARY:
                {
                    const char* op = xl6IrCgenOperator(instruction->opcode);
                    if ( op == NULL || instruction->operandCount != 1 )
                    return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter has no deterministic unary operator rule");
                xl6OutputPrintf(out, "    v%u = %s v%u;\n", (unsigned int)instruction->result, op,
                                (unsigned int)module->operands[instruction->firstOperand]);
                }
                break;
            case XL6_IR_INSTR_VALUE:
                if ( instruction->operandCount != 1 ||
                     ((instruction->result == XL6_IR_VALUE_INVALID) ==
                      (instruction->targetValue == XL6_IR_VALUE_INVALID)) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR value copy has no deterministic definition or merge target");
                xl6OutputPrintf(out, "    v%u = v%u;\n",
                                (unsigned int)(instruction->result != XL6_IR_VALUE_INVALID
                                                   ? instruction->result : instruction->targetValue),
                                (unsigned int)module->operands[instruction->firstOperand]);
                break;
            case XL6_IR_INSTR_DISCARD:
                if ( !xl6IrCgenEmitSimpleObservation(
                        out, module, instruction, diags) ) return 0;
                break;
            case XL6_IR_INSTR_RUNTIME_CALL:
                if ( !xl6IrCgenEmitRuntimeCallInstruction(
                        out, module, instruction, instructionIndex, diags) )
                    return 0;
                break;
            case XL6_IR_INSTR_RELEASE:
            case XL6_IR_INSTR_VALUE_RELEASE:
                {
                if (instruction->kind == XL6_IR_INSTR_VALUE_RELEASE) {
                    if (!xl6IrCgenEmitValueRelease(out, module, instruction, diags)) return 0;
                    break;
                }
                const XL6_IrStorage* storage = xl6IrStorageGet(
                    module, instruction->targetSymbol);
                int indirect = xl6IrCgenParameterAbiIsIndirect(
                    xl6IrCgenStorageParameterAbi(module, storage, NULL));
                const XL6_IrType* releasedType = xl6IrTypeGet(
                    &module->types, instruction->type);
                if ( storage == NULL || xl6IrCgenType(module, instruction->type) == NULL )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter ownership runtime ABI is not implemented");
                if ( releasedType->kind == XL6_IR_TYPE_TUPLE &&
                     releasedType->cAbi == XL6_IR_C_ABI_PRODUCT &&
                     xl6IrCgenProductDropNeeded(
                         module, instruction->type) ) {
                    xl6OutputPrintf(out, "    xlir_product_drop_t%u(",
                        (unsigned int)instruction->type);
                    if ( !indirect ) xl6OutputPuts("&", out);
                    xl6IrCgenPutStorageName(
                        out, module, instruction->targetSymbol);
                    xl6OutputPuts(");\n", out);
                    break;
                }
                if ( !xl6IrCgenEmitSymbolRelease(out, module, instruction->type,
                         instruction->targetSymbol, indirect) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR storage release has no exact detached ownership ABI");
                break;
                }
            case XL6_IR_INSTR_RETAIN:
                if ( !xl6IrCgenEmitRetainInstruction(
                         out, module, instruction) )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR C emitter retain ABI is unresolved");
                break;
            case XL6_IR_INSTR_MOVE:
            case XL6_IR_INSTR_STORAGE_TAKE:
                if (!xl6IrCgenEmitMoveInstruction(out, module, instruction, SIZE_MAX, SIZE_MAX, diags)) return 0;
                break;
            case XL6_IR_INSTR_TRAP_IF_FALSE:
                if ( !xl6IrCgenEmitSimpleObservation(
                        out, module, instruction, diags) ) return 0;
                break;
            case XL6_IR_INSTR_BRANCH:
                xl6OutputPrintf(out, "    if (v%u) goto b%u; else goto b%u;\n",
                                (unsigned int)module->operands[instruction->firstOperand],
                                (unsigned int)instruction->trueTarget, (unsigned int)instruction->falseTarget);
                break;
            case XL6_IR_INSTR_JUMP:
                xl6OutputPrintf(out, "    goto b%u;\n", (unsigned int)instruction->trueTarget);
                break;
            case XL6_IR_INSTR_RETURN:
                /* Replaces the old "XLIR module initializer return ABI is not defined"
                 * rejection: zero-operand return is the explicit success ABI. */
                if ( function->kind == XL6_IR_FUNCTION_MODULE_INIT ) {
                    if ( instruction->operandCount != 0 )
                        return xl6IrCgenDiag(diags, module, instruction->origin,
                                             "XLIR module initializer return has an invalid ABI");
                    xl6OutputPrintf(out, "    xlir_module_initialized = %d;\n",
                        module->abi.finalizerFunction != 0 ? 2 : 1);
                    xl6OutputPuts("    return 1;\n", out);
                } else if ( function->kind == XL6_IR_FUNCTION_MODULE_FINI ) {
                    if (module->abi.taskPoolAbi != XL6_IR_MODULE_TASK_POOL_NONE)
                        xl6OutputPuts("    xlir_async_pool_release();\n", out);
                    xl6OutputPuts("    xlir_module_initialized = 0;\n", out);
                    if (function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE)
                        xl6OutputPrintf(out, "    return (%s){ true };\n",
                            xl6IrCgenErrorResultType(module, XL6_IR_TYPE_INVALID));
                    else xl6OutputPuts("    return;\n", out);
                } else if ( function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE ) {
                    const char* resultType = xl6IrCgenErrorResultType(module, function->returnType);
                    if ( resultType == NULL ||
                         (function->returnType == XL6_IR_TYPE_INVALID
                            ? instruction->operandCount != 0 : instruction->operandCount != 1) )
                        return xl6IrCgenDiag(diags, module, instruction->origin,
                                             "XLIR C emitter error-result return ABI is unresolved");
                    if ( function->returnType == XL6_IR_TYPE_INVALID )
                        xl6OutputPrintf(out, "    return (%s){ true };\n", resultType);
                    else
                        xl6OutputPrintf(out, "    return (%s){ true, v%u };\n", resultType,
                                        (unsigned int)module->operands[instruction->firstOperand]);
                } else if ( function->returnType == XL6_IR_TYPE_INVALID ) {
                    if ( instruction->operandCount != 0 ) return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR void function return carries a value");
                    xl6OutputPuts("    return;\n", out);
                } else {
                    if ( instruction->operandCount != 1 ) return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter requires a single lowered return value");
                    xl6OutputPrintf(out, "    return v%u;\n", (unsigned int)module->operands[instruction->firstOperand]);
                }
                break;
            case XL6_IR_INSTR_THROW:
                if ( function->kind == XL6_IR_FUNCTION_MODULE_INIT ) {
                    if ( module->abi.finalizerFunction != 0 )
                        xl6OutputPuts(xl6IrModuleHasFinalizerWork(module)
                            ? "    xlir_module_fini_after_failure();\n" : "    xlir_module_fini();\n", out);
                    xl6OutputPuts("    return 0;\n", out);
                } else {
                    const char* resultType = xl6IrCgenErrorResultType(module, function->returnType);
                    if ( function->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE || resultType == NULL )
                        return xl6IrCgenDiag(diags, module, instruction->origin,
                                             "XLIR C emitter runtime exception ABI is not implemented");
                    if (function->kind == XL6_IR_FUNCTION_MODULE_FINI) {
                        if (module->abi.taskPoolAbi != XL6_IR_MODULE_TASK_POOL_NONE)
                            xl6OutputPuts("    xlir_async_pool_release();\n", out);
                        xl6OutputPuts("    xlir_module_initialized = 0;\n", out);
                    }
                    xl6OutputPrintf(out, "    return (%s){ false };\n", resultType);
                }
                break;
            case XL6_IR_INSTR_ERROR_TEST:
                {
                    size_t callIndex;
                    XL6_IrValueId value = module->operands[instruction->firstOperand];
                    const XL6_IrInstruction* source = xl6IrCgenValueDefinition(module, value, &callIndex);
                    if ( source == NULL )
                        return xl6IrCgenDiag(diags, module, instruction->origin,
                                             "XLIR C emitter error-result test has no source carrier");
                    if (source->kind == XL6_IR_INSTR_CLEANUP_END) {
                        xl6OutputPrintf(out, "    if (v%u) goto b%u; else goto b%u;\n",
                            (unsigned int)value, (unsigned int)instruction->trueTarget,
                            (unsigned int)instruction->falseTarget);
                    } else if (xl6IrCallInstructionErrorAbi(module, source) == XL6_IR_ERROR_ABI_MAY_RAISE &&
                               xl6IrCallInstructionErrorPhysicalAbi(module, source) != XL6_IR_CALL_ERROR_PHYSICAL_INVALID) {
                        if (!xl6IrCgenEmitCallErrorTest(out, module, source, callIndex, "b",
                                instruction->trueTarget, instruction->falseTarget, diags)) return 0;
                    } else if ( source->kind == XL6_IR_INSTR_CLOSURE_CALL ) {
                        xl6OutputPrintf(out, "    if (c%lu) goto b%u; else goto b%u;\n",
                                        (unsigned long)callIndex,
                                        (unsigned int)instruction->trueTarget,
                                        (unsigned int)instruction->falseTarget);
                    } else if ( source->kind == XL6_IR_INSTR_AWAIT ) {
                        xl6OutputPrintf(out, "    if (await_ok_%u) goto b%u; else goto b%u;\n", (unsigned int)value,
                                        (unsigned int)instruction->trueTarget, (unsigned int)instruction->falseTarget);
                    } else if ( source->kind == XL6_IR_INSTR_GENERATOR_NEXT ) {
                        xl6IrCgenEmitGeneratorNextTest(out, callIndex, value, "b",
                            instruction->trueTarget, instruction->falseTarget);
                    } else if ( source->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                                 (source->runtimeOp == XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED ||
                                 source->runtimeOp == XL6_IR_RUNTIME_STRING_TO_F64_CHECKED ||
                                 source->runtimeOp == XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED ||
                                 source->runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED ||
                                  source->runtimeOp ==
                                      XL6_IR_RUNTIME_XVALUE_CAST_CHECKED ||
                                  source->runtimeOp ==
                                      XL6_IR_RUNTIME_XVALUE_UNION_CAST_CHECKED ||
                                  source->runtimeOp ==
                                      XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED ||
                                  source->runtimeOp ==
                                      XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED ||
                                 source->runtimeOp ==
                                     XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_STRING ||
                                 xl6IrCgenCheckedIntegerRuntimeOpIs(
                                     source->runtimeOp) ||
                                 source->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_DIV ||
                                 source->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MOD ||
                                 source->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHL ||
                                 source->runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHR ||
                                 source->runtimeOp == XL6_IR_RUNTIME_XVALUE_WEAK_CREATE ||
                                 source->runtimeOp == XL6_IR_RUNTIME_CHANNEL_CREATE ||
                                  source->runtimeOp == XL6_IR_RUNTIME_CHANNEL_SEND ||
                                 source->runtimeOp ==
                                      XL6_IR_RUNTIME_CHANNEL_TRY_SEND ||
                                 source->runtimeOp ==
                                      XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE ||
                                  source->runtimeOp ==
                                       XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_PUSH ||
                                  source->runtimeOp ==
                                       XL6_IR_RUNTIME_STACK_CREATE ||
                                  source->runtimeOp ==
                                       XL6_IR_RUNTIME_STACK_PUSH ||
                                  source->runtimeOp ==
                                       XL6_IR_RUNTIME_STACK_PEEK_INTO ||
                                  source->runtimeOp ==
                                       XL6_IR_RUNTIME_STACK_PEEK_AT_INTO ||
                                  source->runtimeOp ==
                                       XL6_IR_RUNTIME_STACK_RESERVE ||
                                  source->runtimeOp ==
                                       XL6_IR_RUNTIME_STACK_CLONE ||
                                  source->runtimeOp ==
                                       XL6_IR_RUNTIME_STACK_TRIM ||
                                  xl6IrCgenAvlRuntimeOpMayRaise(
                                      source->runtimeOp) ||
                                  xl6IrCgenSyncRuntimeOpCreates(
                                      source->runtimeOp)) ) {
                        xl6OutputPrintf(out, "    if (xrtGetError() == NULL) goto b%u; else goto b%u;\n",
                                        (unsigned int)instruction->trueTarget, (unsigned int)instruction->falseTarget);
                    } else if ( source->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                                (source->runtimeOp == XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE ||
                                 source->runtimeOp == XL6_IR_RUNTIME_XVALUE_NULLABLE_CLOSURE_BOX ||
                                 source->runtimeOp == XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL ||
                                 source->runtimeOp ==
                                     XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL ||
                                 source->runtimeOp ==
                                     XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST ||
                                 source->runtimeOp ==
                                     XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND ||
                                  source->runtimeOp ==
                                      XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED ||
                                  source->runtimeOp == XL6_IR_RUNTIME_FUTURE_ALL_XVALUES ||
                                  source->runtimeOp == XL6_IR_RUNTIME_FUTURE_RACE ||
                                  source->runtimeOp ==
                                      XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY ||
                                  source->runtimeOp ==
                                      XL6_IR_RUNTIME_FUTURE_RACE_ARRAY ||
                                  source->runtimeOp ==
                                      XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX ||
                                  source->runtimeOp ==
                                      XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY ||
                                   source->runtimeOp ==
                                       XL6_IR_RUNTIME_THREAD_CREATE ||
                                   source->runtimeOp ==
                                       XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE ||
                                   source->runtimeOp ==
                                       XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE) ) {
                        xl6OutputPrintf(out, "    if (v%u != NULL) goto b%u; else goto b%u;\n",
                                        (unsigned int)value,
                                        (unsigned int)instruction->trueTarget,
                                        (unsigned int)instruction->falseTarget);
                    } else {
                        return xl6IrCgenDiag(diags, module, instruction->origin,
                                             "XLIR C emitter error-result test has no supported source ABI");
                    }
                }
                break;
            case XL6_IR_INSTR_AWAIT:
                if ( !xl6IrCgenEmitBlockingAwaitInstruction(
                        out, module, instruction, diags,
                        "XLIR C emitter await result ABI is not implemented") ) return 0;
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
                if (!xl6IrCgenEmitErrorStateInstruction(out, module, instruction))
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                        "XLIR error-state ownership ABI is unresolved");
                break;
            default:
                return xl6IrCgenDiag(diags, module, instruction->origin, "XLIR C emitter has no rule for this lowered instruction");
    }
    return !xl6OutputHasFailed(out);
}
