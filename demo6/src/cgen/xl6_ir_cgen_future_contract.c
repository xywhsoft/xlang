static unsigned int xl6IrCgenFutureMemberMask(XL6_IrRuntimeOp operation)
{
    switch ( operation ) {
    case XL6_IR_RUNTIME_FUTURE_WAIT: return 1u << 0;
    case XL6_IR_RUNTIME_FUTURE_WAIT_TIMEOUT: return 1u << 1;
    case XL6_IR_RUNTIME_FUTURE_WAIT_UNTIL: return 1u << 2;
    case XL6_IR_RUNTIME_FUTURE_IS_DONE: return 1u << 3;
    case XL6_IR_RUNTIME_FUTURE_IS_PENDING: return 1u << 4;
    case XL6_IR_RUNTIME_FUTURE_IS_CANCELLED: return 1u << 5;
    case XL6_IR_RUNTIME_FUTURE_IS_FAILED: return 1u << 6;
    case XL6_IR_RUNTIME_FUTURE_STATE: return 1u << 7;
    case XL6_IR_RUNTIME_FUTURE_STATUS: return 1u << 8;
    case XL6_IR_RUNTIME_FUTURE_ERROR_REF: return 1u << 9;
    case XL6_IR_RUNTIME_FUTURE_SET_DEBUG_NAME: return 1u << 10;
    case XL6_IR_RUNTIME_FUTURE_DEBUG_NAME_COPY: return 1u << 11;
    case XL6_IR_RUNTIME_FUTURE_CREATE_TIME_MS: return 1u << 12;
    case XL6_IR_RUNTIME_FUTURE_COMPLETE_TIME_MS: return 1u << 13;
    case XL6_IR_RUNTIME_FUTURE_PENDING_CONTINUATIONS: return 1u << 14;
    default: return 0;
    }
}

static int xl6IrCgenRuntimeFutureMemberAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrInstruction* argumentValue = NULL;
    const XL6_IrType* sourceType;
    const XL6_IrType* argumentType = NULL;
    const XL6_IrType* resultType;
    int takesTime;
    int takesName;
    int returnsName;
    int returnsI64;
    if ( module == NULL || instruction == NULL ||
         xl6IrCgenFutureMemberHelper(instruction->runtimeOp) == NULL ) return 0;
    takesTime = instruction->runtimeOp ==
            XL6_IR_RUNTIME_FUTURE_WAIT_TIMEOUT ||
        instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_WAIT_UNTIL;
    takesName = instruction->runtimeOp ==
        XL6_IR_RUNTIME_FUTURE_SET_DEBUG_NAME;
    returnsName = instruction->runtimeOp ==
        XL6_IR_RUNTIME_FUTURE_DEBUG_NAME_COPY;
    returnsI64 = instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_STATE ||
        instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_STATUS ||
        instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_CREATE_TIME_MS ||
        instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_COMPLETE_TIME_MS ||
        instruction->runtimeOp ==
            XL6_IR_RUNTIME_FUTURE_PENDING_CONTINUATIONS;
    if ( instruction->operandCount != ((takesTime || takesName) ? 2u : 1u) ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    if ( takesTime || takesName )
        argumentValue = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + 1], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    if ( takesTime || takesName )
        argumentType = argumentValue != NULL
            ? xl6IrTypeGet(&module->types, argumentValue->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return sourceType != NULL &&
           sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
           sourceType->paramCount == 1 &&
           (!takesTime || (argumentType != NULL &&
                            argumentType->cAbi == XL6_IR_C_ABI_I64)) &&
           (!takesName || (argumentType != NULL &&
                            argumentType->cAbi ==
                                XL6_IR_C_ABI_MANAGED_STRING &&
                            argumentType->ownershipAbi ==
                                XL6_IR_OWNERSHIP_ABI_MANAGED)) &&
           resultType != NULL &&
           (returnsI64
                ? resultType->cAbi == XL6_IR_C_ABI_I64
                : returnsName
                    ? resultType->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
                      resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED
                : instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_ERROR_REF
                    ? resultType->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
                      resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED
                    : resultType->cAbi == XL6_IR_C_ABI_BOOL);
}

static int xl6IrCgenEmitFutureMemberRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const char* helper;
    if ( instruction == NULL ||
         (helper = xl6IrCgenFutureMemberHelper(
              instruction->runtimeOp)) == NULL ) return 0;
    if ( !xl6IrCgenRuntimeFutureMemberAbi(module, instruction) ) return -1;
    if ( instruction->operandCount == 2 )
        xl6OutputPrintf(out, "    v%u = %s(v%u, v%u);\n",
            (unsigned int)instruction->result, helper,
            (unsigned int)module->operands[instruction->firstOperand],
            (unsigned int)module->operands[instruction->firstOperand + 1]);
    else
        xl6OutputPrintf(out, "    v%u = %s(v%u);\n",
            (unsigned int)instruction->result, helper,
            (unsigned int)module->operands[instruction->firstOperand]);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenAsyncPayloadAbiMatches(
    const XL6_IrType* payload, XL6_IrAsyncResultAbi abi)
{
    if ( payload == NULL ) return 0;
    switch ( abi ) {
    case XL6_IR_ASYNC_RESULT_VOID:
        return payload->cAbi == XL6_IR_C_ABI_UNSUPPORTED &&
               payload->name != NULL && strcmp(payload->name, "null") == 0;
    case XL6_IR_ASYNC_RESULT_COPY_BOOL:
        return payload->cAbi == XL6_IR_C_ABI_BOOL;
    case XL6_IR_ASYNC_RESULT_COPY_CHAR:
        return payload->cAbi == XL6_IR_C_ABI_CHAR_U32;
    case XL6_IR_ASYNC_RESULT_COPY_I64:
        return payload->cAbi == XL6_IR_C_ABI_I64;
    case XL6_IR_ASYNC_RESULT_COPY_U64:
        return payload->cAbi == XL6_IR_C_ABI_U64;
    case XL6_IR_ASYNC_RESULT_COPY_TYPE_ID:
        return payload->cAbi == XL6_IR_C_ABI_TYPE_ID;
    case XL6_IR_ASYNC_RESULT_COPY_F64:
        return payload->cAbi == XL6_IR_C_ABI_F64;
    case XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER:
        return payload->cAbi == XL6_IR_C_ABI_RAW_POINTER &&
               payload->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    case XL6_IR_ASYNC_RESULT_OWNED_STRING:
        return payload->cAbi == XL6_IR_C_ABI_MANAGED_STRING;
    case XL6_IR_ASYNC_RESULT_RETAINED_XVALUE:
        return payload->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
               payload->cAbi == XL6_IR_C_ABI_MANAGED_BYTES;
    case XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE:
        return payload->cAbi == XL6_IR_C_ABI_CLOSURE_PTR;
    case XL6_IR_ASYNC_RESULT_RETAINED_ERROR:
        return payload->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
               payload->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
               payload->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL &&
               payload->typeValueId != 0;
    case XL6_IR_ASYNC_RESULT_RETAINED_FUTURE:
        return payload->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR;
    case XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR:
        return payload->cAbi == XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR;
    default:
        return 0;
    }
}

/* AWAIT already carries its concrete Future payload transfer contract in
 * asyncResultAbi.  Keep this check shared by ordinary code and the synchronous
 * generator-resume backend so neither emitter has to rediscover the payload
 * representation from source syntax. */
static int xl6IrCgenBlockingAwaitAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* future;
    const XL6_IrType* futureType;
    const XL6_IrType* resultType;
    int resultMatches;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_AWAIT ||
         instruction->operandCount != 1 ||
         instruction->result == XL6_IR_VALUE_INVALID ) return 0;
    future = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    futureType = future != NULL
        ? xl6IrTypeGet(&module->types, future->type) : NULL;
    if ( futureType == NULL ||
         futureType->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
         futureType->paramCount != 1 ) return 0;
    if ( instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID )
        return instruction->type == XL6_IR_TYPE_INVALID &&
               xl6IrCgenAsyncPayloadAbiMatches(
                   xl6IrTypeGet(&module->types, futureType->params[0]),
                   XL6_IR_ASYNC_RESULT_VOID);
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    resultMatches = instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_TAGGED
        ? resultType != NULL && resultType->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE
        : xl6IrCgenAsyncPayloadAbiMatches(
              resultType, instruction->asyncResultAbi);
    return resultMatches && futureType->params[0] == instruction->type;
}

/* One typed acquisition transaction serves blocking and resumed AWAIT.
 * The payload and status are separate physical outputs of the verified ABI. */
static int xl6IrCgenEmitAwaitPayload(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, const char* payload)
{
    XL6_IrAsyncResultAbi abi = instruction->asyncResultAbi;
    const char* type = xl6IrCgenType(module, instruction->type);
    const char* retain = abi == XL6_IR_ASYNC_RESULT_RETAINED_XVALUE ? "xrtValueRetain" :
        abi == XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE ? "xlir_closure_retain" :
        abi == XL6_IR_ASYNC_RESULT_RETAINED_ERROR ? "xrtErrorRef" :
        abi == XL6_IR_ASYNC_RESULT_RETAINED_FUTURE ? "xrtFutureRef" :
        abi == XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR ? "xlir_generator_retain" :
        abi == XL6_IR_ASYNC_RESULT_OWNED_STRING ? "xlir_string_copy" : NULL;
    unsigned int result = (unsigned int)instruction->result;
    if (instruction->awaitOutcomeAbi !=
        XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR) return 0;
    if (abi == XL6_IR_ASYNC_RESULT_VOID) return 1;
    if (type == NULL) return 0;
    xl6OutputPrintf(out, "    if (await_ok_%u) {\n"
        "      %s const* payload = (%s const*)(%s);\n"
        "      if (payload == NULL) { await_ok_%u = false; xrtSetErrorKind(XERR_STATE); }\n"
        "      else {\n", result, type, type, payload, result);
    if (abi == XL6_IR_ASYNC_RESULT_OWNED_STRING)
        xl6OutputPrintf(out, "        v%u = xlir_string_copy(*payload);\n"
            "        if (payload->Data != NULL && v%u.Data == NULL) await_ok_%u = false;\n",
            result, result, result);
    else if (retain != NULL)
        xl6OutputPrintf(out, "        v%u = *payload != NULL ? %s(*payload) : NULL;\n"
            "        if (*payload != NULL && v%u == NULL) await_ok_%u = false;\n",
            result, retain, result, result);
    else xl6OutputPrintf(out, "        v%u = *payload;\n", result);
    xl6OutputPuts("      }\n    }\n", out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitBlockingAwaitInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, XL6_DiagList* diags,
    const char* unresolvedMessage)
{
    char payload[80];
    unsigned int future;
    if (!xl6IrCgenBlockingAwaitAbi(module, instruction) ||
        instruction->awaitOutcomeAbi !=
            XL6_IR_AWAIT_OUTCOME_STATUS_DIAGNOSTIC_NULL_STATE_ERROR)
        return xl6IrCgenDiag(diags, module, instruction->origin, unresolvedMessage);
    future = (unsigned int)module->operands[instruction->firstOperand];
    xl6OutputPrintf(out,
        "    bool await_ok_%u = xlir_future_wait_checked(v%u, %d, %d);\n",
        (unsigned int)instruction->result, future,
        instruction->sourceSpan.line, instruction->sourceSpan.column);
    snprintf(payload, sizeof(payload), "xrtFutureValue(v%u)", future);
    return xl6IrCgenEmitAwaitPayload(out, module, instruction, payload);
}

static int xl6IrCgenFuturePayloadBoxerAbiSupported(
    XL6_IrAsyncResultAbi abi)
{
    switch ( abi ) {
    case XL6_IR_ASYNC_RESULT_COPY_BOOL:
    case XL6_IR_ASYNC_RESULT_COPY_CHAR:
    case XL6_IR_ASYNC_RESULT_COPY_I64:
    case XL6_IR_ASYNC_RESULT_COPY_U64:
    case XL6_IR_ASYNC_RESULT_COPY_TYPE_ID:
    case XL6_IR_ASYNC_RESULT_COPY_F64:
    case XL6_IR_ASYNC_RESULT_OWNED_STRING:
    case XL6_IR_ASYNC_RESULT_RETAINED_XVALUE:
    case XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE:
    case XL6_IR_ASYNC_RESULT_RETAINED_ERROR:
    case XL6_IR_ASYNC_RESULT_RETAINED_FUTURE:
        return 1;
    default:
        return 0;
    }
}

static int xl6IrCgenRuntimeTaskSpawnAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* closureValue;
    const XL6_IrType* signature;
    const XL6_IrType* resultFuture;
    const XL6_IrType* payload;
    size_t argument;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE ||
         instruction->operandCount < 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->calleeType == XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_INVALID ) return 0;
    closureValue = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    signature = xl6IrTypeGet(&module->types, instruction->calleeType);
    resultFuture = xl6IrTypeGet(&module->types, instruction->type);
    payload = xl6IrTypeGet(&module->types, instruction->payloadType);
    if ( closureValue == NULL ||
         closureValue->type != instruction->calleeType ||
         signature == NULL || signature->kind != XL6_IR_TYPE_FUNCTION ||
         signature->cAbi != XL6_IR_C_ABI_CLOSURE_PTR ||
         signature->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
         instruction->callableErasureAbi !=
             signature->callableErasureAbi ||
         (instruction->callableErasureAbi !=
              XL6_IR_CALLABLE_ERASURE_FIXED &&
          instruction->callableErasureAbi !=
              XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC) ||
         signature->paramCount + 1 != instruction->operandCount ||
         ((instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID &&
           (signature->returnCount != 0 ||
            signature->callableResultType != XL6_IR_TYPE_INVALID)) ||
          (instruction->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID &&
           (signature->returnCount != 1 ||
            signature->callableResultType != instruction->payloadType))) ||
         (signature->paramCount != 0 &&
          (signature->parameterModes == NULL ||
           signature->parameterAbis == NULL)) ||
         resultFuture == NULL ||
         resultFuture->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
         resultFuture->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
         resultFuture->paramCount != 1 || resultFuture->params == NULL ||
         resultFuture->params[0] != instruction->payloadType ||
         !xl6IrCgenAsyncPayloadAbiMatches(
             payload, instruction->asyncResultAbi) ) return 0;
    for ( argument = 0; argument < signature->paramCount; ++argument ) {
        const XL6_IrInstruction* value = xl6IrCgenValueDefinition(
            module, module->operands[
                instruction->firstOperand + argument + 1], NULL);
        const XL6_IrType* parameter = xl6IrTypeGet(
            &module->types, signature->params[argument]);
        if ( value == NULL || parameter == NULL ||
             !xl6IrTaskSpawnParameterAbiSupported(
                 parameter, signature->parameterModes[argument], signature->parameterAbis[argument]) ||
             value->type != signature->params[argument] ) return 0;
    }
    return 1;
}

static int xl6IrCgenRuntimeFutureContinuationAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* sourceValue;
    const XL6_IrInstruction* closureValue;
    const XL6_IrType* sourceFuture;
    const XL6_IrType* resultFuture;
    const XL6_IrType* signature;
    const XL6_IrType* sourcePayload;
    const XL6_IrType* outputPayload;
    const XL6_IrType* parameter = NULL;
    int isThen;
    int isCatch;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE ||
         instruction->operandCount != 2 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->calleeType == XL6_IR_TYPE_INVALID ||
         instruction->sourcePayloadType == XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->sourceAsyncResultAbi == XL6_IR_ASYNC_RESULT_INVALID ||
         instruction->sourceAsyncResultAbi == XL6_IR_ASYNC_RESULT_VOID ||
         instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_INVALID ||
         instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID ||
         instruction->futureContinuationKind <
             XL6_IR_FUTURE_CONTINUATION_THEN ||
         instruction->futureContinuationKind >
             XL6_IR_FUTURE_CONTINUATION_FINALLY ) return 0;
    sourceValue = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    closureValue = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand + 1], NULL);
    sourceFuture = sourceValue != NULL
        ? xl6IrTypeGet(&module->types, sourceValue->type) : NULL;
    resultFuture = xl6IrTypeGet(&module->types, instruction->type);
    signature = xl6IrTypeGet(&module->types, instruction->calleeType);
    sourcePayload = xl6IrTypeGet(
        &module->types, instruction->sourcePayloadType);
    outputPayload = xl6IrTypeGet(&module->types, instruction->payloadType);
    if ( signature != NULL && signature->paramCount == 1 &&
         signature->params != NULL )
        parameter = xl6IrTypeGet(&module->types, signature->params[0]);
    isThen = instruction->futureContinuationKind ==
        XL6_IR_FUTURE_CONTINUATION_THEN;
    isCatch = instruction->futureContinuationKind ==
        XL6_IR_FUTURE_CONTINUATION_CATCH;
    if ( sourceValue == NULL || closureValue == NULL ||
         sourceFuture == NULL ||
         sourceFuture->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
         sourceFuture->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
         sourceFuture->paramCount != 1 || sourceFuture->params == NULL ||
         sourceFuture->params[0] != instruction->sourcePayloadType ||
         resultFuture == NULL ||
         resultFuture->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
         resultFuture->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
         resultFuture->paramCount != 1 || resultFuture->params == NULL ||
         resultFuture->params[0] != instruction->payloadType ||
         closureValue->type != instruction->calleeType ||
         signature == NULL || signature->kind != XL6_IR_TYPE_FUNCTION ||
         signature->cAbi != XL6_IR_C_ABI_CLOSURE_PTR ||
         signature->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
         !xl6IrCgenAsyncPayloadAbiMatches(
             sourcePayload, instruction->sourceAsyncResultAbi) ||
         !xl6IrCgenAsyncPayloadAbiMatches(
             outputPayload, instruction->asyncResultAbi) ) return 0;
    if ( isThen )
        return signature->paramCount == 1 && parameter != NULL &&
            signature->parameterModes != NULL &&
            signature->parameterAbis != NULL &&
            signature->parameterModes[0] == XL6_IR_PARAMETER_MODE_DEFAULT &&
            signature->parameterAbis[0] ==
                xl6IrParameterAbiForMode(
                    parameter, XL6_IR_PARAMETER_MODE_DEFAULT) &&
            signature->params[0] == instruction->sourcePayloadType &&
            signature->returnCount == 1 &&
            signature->callableResultType == instruction->payloadType;
    if ( isCatch )
        return instruction->sourcePayloadType == instruction->payloadType &&
            signature->paramCount == 1 && parameter != NULL &&
            parameter->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
            parameter->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            signature->parameterModes != NULL &&
            signature->parameterAbis != NULL &&
            signature->parameterModes[0] == XL6_IR_PARAMETER_MODE_DEFAULT &&
            signature->parameterAbis[0] == XL6_IR_PARAMETER_ABI_BORROW &&
            signature->returnCount == 1 &&
            signature->callableResultType == instruction->payloadType;
    return instruction->sourcePayloadType == instruction->payloadType &&
        signature->paramCount == 0 && signature->returnCount == 0 &&
        signature->callableResultType == XL6_IR_TYPE_INVALID;
}

static int xl6IrCgenRuntimeFutureAggregateAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* result;
    const XL6_IrType* payload;
    size_t i;
    if ( module == NULL || instruction == NULL || instruction->operandCount == 0 ||
         (instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
          instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_RACE) ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    result = xl6IrTypeGet(&module->types, instruction->type);
    payload = xl6IrTypeGet(&module->types, instruction->payloadType);
    if ( result == NULL || result->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
         result->paramCount != 1 ||
         !xl6IrCgenAsyncPayloadAbiMatches(payload, instruction->asyncResultAbi) ||
         (instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_ALL_XVALUES &&
          instruction->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID &&
          !xl6IrCgenFuturePayloadBoxerAbiSupported(
              instruction->asyncResultAbi)) ||
         (instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE &&
          !xl6IrCgenRuntimeCallableSignatureAbi(module, instruction->payloadType)) )
        return 0;
    for ( i = 0; i < instruction->operandCount; ++i ) {
        const XL6_IrInstruction* source = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + i], NULL);
        const XL6_IrType* future = source != NULL
            ? xl6IrTypeGet(&module->types, source->type) : NULL;
        if ( future == NULL ||
             future->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
             future->paramCount != 1 ) return 0;
    }
    return 1;
}

static int xl6IrCgenRuntimeFutureWhenAnyIndexAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* result;
    const XL6_IrType* payload;
    size_t i;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX ||
         instruction->operandCount == 0 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->asyncResultAbi != XL6_IR_ASYNC_RESULT_COPY_I64 ) return 0;
    result = xl6IrTypeGet(&module->types, instruction->type);
    payload = xl6IrTypeGet(&module->types, instruction->payloadType);
    if ( result == NULL || result->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
         result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
         result->paramCount != 1 || result->params == NULL ||
         result->params[0] != instruction->payloadType || payload == NULL ||
         payload->cAbi != XL6_IR_C_ABI_I64 ||
         payload->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ) return 0;
    for ( i = 0; i < instruction->operandCount; ++i ) {
        const XL6_IrInstruction* source = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + i], NULL);
        const XL6_IrType* future = source != NULL
            ? xl6IrTypeGet(&module->types, source->type) : NULL;
        if ( future == NULL ||
             future->cAbi != XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ||
             future->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
             future->paramCount != 1 || future->params == NULL ||
             xl6IrTypeGet(&module->types, future->params[0]) == NULL ) return 0;
    }
    return 1;
}

static int xl6IrCgenRuntimeFutureAggregateArrayAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* result;
    const XL6_IrType* payload;
    const XL6_IrInstruction* source;
    const XL6_IrType* arrayType;
    const XL6_IrType* inputFuture;
    XL6_IrTypeId publicPayload;
    int shapeMatches;
    if ( module == NULL || instruction == NULL ||
         (instruction->runtimeOp !=
              XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY &&
          instruction->runtimeOp != XL6_IR_RUNTIME_FUTURE_RACE_ARRAY) ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_INVALID ) return 0;
    result = xl6IrTypeGet(&module->types, instruction->type);
    payload = xl6IrTypeGet(&module->types, instruction->payloadType);
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    arrayType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    inputFuture = arrayType != NULL && arrayType->paramCount == 1 &&
        arrayType->params != NULL
            ? xl6IrTypeGet(&module->types, arrayType->params[0]) : NULL;
    publicPayload = result != NULL && result->paramCount == 1 &&
        result->params != NULL ? result->params[0] : XL6_IR_TYPE_INVALID;
    shapeMatches = result != NULL &&
        result->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
        result->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        publicPayload != XL6_IR_TYPE_INVALID &&
        xl6IrCgenAsyncPayloadAbiMatches(
            payload, instruction->asyncResultAbi) &&
        !(instruction->asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE &&
          !xl6IrCgenRuntimeCallableSignatureAbi(
              module, instruction->payloadType));
    if ( shapeMatches && instruction->runtimeOp ==
             XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY &&
         instruction->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID &&
         !xl6IrCgenFuturePayloadBoxerAbiSupported(
             instruction->asyncResultAbi) ) shapeMatches = 0;
    if ( shapeMatches && instruction->runtimeOp ==
         XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY &&
         instruction->asyncResultAbi != XL6_IR_ASYNC_RESULT_VOID ) {
        const XL6_IrType* resultArray =
            xl6IrTypeGet(&module->types, publicPayload);
        shapeMatches = resultArray != NULL &&
            resultArray->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
            resultArray->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
            resultArray->collectionAbi == XL6_IR_COLLECTION_ARRAY &&
            resultArray->paramCount == 1 && resultArray->params != NULL &&
            resultArray->params[0] == instruction->payloadType;
    } else if ( shapeMatches ) {
        shapeMatches = publicPayload == instruction->payloadType;
    }
    return shapeMatches && arrayType != NULL &&
        arrayType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        arrayType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        arrayType->collectionAbi == XL6_IR_COLLECTION_ARRAY &&
        arrayType->paramCount == 1 && arrayType->params != NULL &&
        inputFuture != NULL &&
        inputFuture->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
        inputFuture->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        inputFuture->paramCount == 1 && inputFuture->params != NULL &&
        inputFuture->params[0] == instruction->payloadType;
}

static int xl6IrCgenRuntimeFutureWhenAnyIndexArrayAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrType* result;
    const XL6_IrType* payload;
    const XL6_IrInstruction* source;
    const XL6_IrType* arrayType;
    const XL6_IrType* elementFuture;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp !=
             XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->asyncResultAbi != XL6_IR_ASYNC_RESULT_COPY_I64 ) return 0;
    result = xl6IrTypeGet(&module->types, instruction->type);
    payload = xl6IrTypeGet(&module->types, instruction->payloadType);
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    arrayType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    elementFuture = arrayType != NULL && arrayType->paramCount == 1 &&
        arrayType->params != NULL
            ? xl6IrTypeGet(&module->types, arrayType->params[0]) : NULL;
    return result != NULL &&
        result->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
        result->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        result->paramCount == 1 && result->params != NULL &&
        result->params[0] == instruction->payloadType &&
        payload != NULL && payload->cAbi == XL6_IR_C_ABI_I64 &&
        payload->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        arrayType != NULL &&
        arrayType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        arrayType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        arrayType->collectionAbi == XL6_IR_COLLECTION_ARRAY &&
        arrayType->paramCount == 1 && arrayType->params != NULL &&
        elementFuture != NULL &&
        elementFuture->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
        elementFuture->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        elementFuture->paramCount == 1 && elementFuture->params != NULL &&
        xl6IrTypeGet(&module->types, elementFuture->params[0]) != NULL;
}

static int xl6IrCgenRuntimeCollectionAlgebraAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* left;
    const XL6_IrInstruction* right;
    if ( module == NULL || instruction == NULL ||
         !xl6IrCollectionAlgebraRuntimeOpIs(instruction->runtimeOp) ||
         instruction->operandCount != 2 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    left = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    right = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand + 1], NULL);
    return left != NULL && right != NULL &&
        xl6IrCgenType(module, instruction->type) != NULL &&
        xl6IrCollectionAlgebraRuntimeSignatureValid(
            &module->types, instruction->runtimeOp, instruction->type,
        left->type, right->type);
}

static int xl6IrCgenEmitCollectionAlgebraInstruction(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    XL6_IrValueId left;
    XL6_IrValueId right;
    const char* function;
    if ( out == NULL ||
         !xl6IrCgenRuntimeCollectionAlgebraAbi(module, instruction) ) return 0;
    left = module->operands[instruction->firstOperand];
    right = module->operands[instruction->firstOperand + 1];
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_XVALUE_ARRAY_CONCAT:
        function = "xl6ArrayConcatV1";
        break;
    case XL6_IR_RUNTIME_XVALUE_LIST_MERGE_RIGHT:
        function = "xl6ListValueMergeRightV1";
        break;
    case XL6_IR_RUNTIME_XVALUE_LIST_WITHOUT_I64_KEY:
        function = "xl6ListValueWithoutKeyV1";
        break;
    case XL6_IR_RUNTIME_XVALUE_DICT_MERGE_RIGHT:
        function = "xlir_xvalue_dict_merge_right";
        break;
    case XL6_IR_RUNTIME_XVALUE_DICT_WITHOUT_STRING_KEY:
        function = "xlir_xvalue_dict_without_string_key";
        break;
    case XL6_IR_RUNTIME_XVALUE_SET_UNION:
    case XL6_IR_RUNTIME_XVALUE_SET_INTERSECTION:
    case XL6_IR_RUNTIME_XVALUE_SET_DIFFERENCE:
    case XL6_IR_RUNTIME_XVALUE_SET_SYMMETRIC_DIFFERENCE:
        xl6OutputPrintf(out,"    v%u = xl6SetValueAlgebraV1(v%u, v%u, %s);\n",
            (unsigned)instruction->result,(unsigned)left,(unsigned)right,
            instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_SET_UNION ? "XL6_SET_UNION_V1" :
            instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_SET_INTERSECTION ? "XL6_SET_INTERSECTION_V1" :
            instruction->runtimeOp==XL6_IR_RUNTIME_XVALUE_SET_DIFFERENCE ? "XL6_SET_DIFFERENCE_V1" : "XL6_SET_SYMMETRIC_DIFFERENCE_V1");
        return 1;
    case XL6_IR_RUNTIME_XVALUE_SET_SUBSET:
    case XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUBSET:
        xl6OutputPrintf(out,
            "    v%u = xl6SetValueRelationV1(v%u, v%u, XL6_SET_SUBSET_V1, %s);\n",
            (unsigned int)instruction->result, (unsigned int)left,
            (unsigned int)right,
            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUBSET
                ? "true" : "false");
        return 1;
    case XL6_IR_RUNTIME_XVALUE_SET_SUPERSET:
    case XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUPERSET:
        xl6OutputPrintf(out,
            "    v%u = xl6SetValueRelationV1(v%u, v%u, XL6_SET_SUPERSET_V1, %s);\n",
            (unsigned int)instruction->result, (unsigned int)left,
            (unsigned int)right,
            instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUPERSET
                ? "true" : "false");
        return 1;
    default:
        return 0;
    }
    xl6OutputPrintf(out, "    v%u = %s(v%u, v%u);\n",
        (unsigned int)instruction->result, function,
        (unsigned int)left, (unsigned int)right);
    return 1;
}
