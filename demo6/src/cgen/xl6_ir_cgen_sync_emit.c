static int xl6IrCgenSyncRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation >= XL6_IR_RUNTIME_MUTEX_CREATE &&
           operation <= XL6_IR_RUNTIME_SEM_CREATE_DEFAULT;
}

static int xl6IrCgenSyncRuntimeOpCreates(XL6_IrRuntimeOp operation)
{
    return operation == XL6_IR_RUNTIME_MUTEX_CREATE ||
           operation == XL6_IR_RUNTIME_SEM_CREATE ||
           operation == XL6_IR_RUNTIME_COND_CREATE ||
           operation == XL6_IR_RUNTIME_RWLOCK_CREATE ||
           operation == XL6_IR_RUNTIME_SEM_CREATE_DEFAULT;
}

static XL6_IrRuntimeResourceAbi xl6IrCgenSyncResourceAbi(
    XL6_IrRuntimeOp operation)
{
    if ( operation >= XL6_IR_RUNTIME_MUTEX_CREATE &&
         operation <= XL6_IR_RUNTIME_MUTEX_UNLOCK )
        return XL6_IR_RUNTIME_RESOURCE_MUTEX;
    if ( (operation >= XL6_IR_RUNTIME_SEM_CREATE &&
          operation <= XL6_IR_RUNTIME_SEM_POST_MULTIPLE) ||
         operation == XL6_IR_RUNTIME_SEM_CREATE_DEFAULT )
        return XL6_IR_RUNTIME_RESOURCE_SEM;
    if ( operation >= XL6_IR_RUNTIME_COND_CREATE &&
         operation <= XL6_IR_RUNTIME_COND_BROADCAST )
        return XL6_IR_RUNTIME_RESOURCE_COND;
    if ( operation >= XL6_IR_RUNTIME_RWLOCK_CREATE &&
         operation <= XL6_IR_RUNTIME_RWLOCK_UPGRADE )
        return XL6_IR_RUNTIME_RESOURCE_RWLOCK;
    return XL6_IR_RUNTIME_RESOURCE_NONE;
}

static int xl6IrCgenSyncTypeAbi(
    const XL6_IrType* type, XL6_IrRuntimeResourceAbi resourceAbi)
{
    return type != NULL && type->kind == XL6_IR_TYPE_BUILTIN &&
           type->paramCount == 0 && type->runtimeResourceAbi == resourceAbi &&
           type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           type->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
           type->typeValueId != 0;
}

static int xl6IrCgenRuntimeSyncAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    int creates;
    XL6_IrRuntimeResourceAbi resourceAbi;
    const XL6_IrInstruction* receiver = NULL;
    const XL6_IrInstruction* second = NULL;
    const XL6_IrInstruction* third = NULL;
    const XL6_IrType* resourceType;
    const XL6_IrType* resultType;
    const XL6_IrType* secondType = NULL;
    const XL6_IrType* thirdType = NULL;
    XL6_IrTypeId resourceTypeId;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenSyncRuntimeOpIs(instruction->runtimeOp) ||
         instruction->result == XL6_IR_VALUE_INVALID ||
         instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeResourcePayloadAbi !=
             XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount >
             module->operandCount - instruction->firstOperand ) return 0;
    creates = xl6IrCgenSyncRuntimeOpCreates(instruction->runtimeOp);
    resourceAbi = xl6IrCgenSyncResourceAbi(instruction->runtimeOp);
    if ( !creates && instruction->operandCount != 0 )
        receiver = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand], NULL);
    if ( instruction->operandCount > 1 )
        second = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + 1], NULL);
    if ( instruction->operandCount > 2 )
        third = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + 2], NULL);
    resourceTypeId = creates ? instruction->type
        : receiver != NULL ? receiver->type : XL6_IR_TYPE_INVALID;
    resourceType = xl6IrTypeGet(&module->types, resourceTypeId);
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    secondType = second != NULL
        ? xl6IrTypeGet(&module->types, second->type) : NULL;
    thirdType = third != NULL
        ? xl6IrTypeGet(&module->types, third->type) : NULL;
    if ( resourceAbi == XL6_IR_RUNTIME_RESOURCE_NONE ||
         !xl6IrCgenSyncTypeAbi(resourceType, resourceAbi) ) return 0;
    if ( creates ) {
        if ( instruction->runtimeOp == XL6_IR_RUNTIME_SEM_CREATE ) {
            const XL6_IrInstruction* first = instruction->operandCount > 0
                ? xl6IrCgenValueDefinition(
                    module, module->operands[instruction->firstOperand], NULL)
                : NULL;
            const XL6_IrType* firstType = first != NULL
                ? xl6IrTypeGet(&module->types, first->type) : NULL;
            return instruction->operandCount == 2 && firstType != NULL &&
                firstType->cAbi == XL6_IR_C_ABI_I64 && secondType != NULL &&
                secondType->cAbi == XL6_IR_C_ABI_I64;
        }
        return instruction->operandCount == 0;
    }
    if ( receiver == NULL || receiver->type != resourceTypeId ||
         resultType == NULL || resultType->cAbi != XL6_IR_C_ABI_BOOL ||
         resultType->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ) return 0;
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_MUTEX_LOCK:
    case XL6_IR_RUNTIME_MUTEX_TRY_LOCK:
    case XL6_IR_RUNTIME_MUTEX_UNLOCK:
    case XL6_IR_RUNTIME_SEM_WAIT:
    case XL6_IR_RUNTIME_SEM_TRY_WAIT:
    case XL6_IR_RUNTIME_SEM_POST:
    case XL6_IR_RUNTIME_COND_SIGNAL:
    case XL6_IR_RUNTIME_COND_BROADCAST:
    case XL6_IR_RUNTIME_RWLOCK_READ_LOCK:
    case XL6_IR_RUNTIME_RWLOCK_TRY_READ_LOCK:
    case XL6_IR_RUNTIME_RWLOCK_READ_UNLOCK:
    case XL6_IR_RUNTIME_RWLOCK_WRITE_LOCK:
    case XL6_IR_RUNTIME_RWLOCK_TRY_WRITE_LOCK:
    case XL6_IR_RUNTIME_RWLOCK_WRITE_UNLOCK:
    case XL6_IR_RUNTIME_RWLOCK_DOWNGRADE:
    case XL6_IR_RUNTIME_RWLOCK_UPGRADE:
        return instruction->operandCount == 1;
    case XL6_IR_RUNTIME_SEM_WAIT_TIMEOUT:
    case XL6_IR_RUNTIME_SEM_POST_MULTIPLE:
        return instruction->operandCount == 2 && secondType != NULL &&
            secondType->cAbi == XL6_IR_C_ABI_I64;
    case XL6_IR_RUNTIME_COND_WAIT:
    case XL6_IR_RUNTIME_COND_WAIT_TIMEOUT:
        return instruction->operandCount ==
                   (instruction->runtimeOp == XL6_IR_RUNTIME_COND_WAIT
                        ? 2u : 3u) &&
            xl6IrCgenSyncTypeAbi(
                secondType, XL6_IR_RUNTIME_RESOURCE_MUTEX) &&
            (instruction->runtimeOp == XL6_IR_RUNTIME_COND_WAIT ||
             (thirdType != NULL && thirdType->cAbi == XL6_IR_C_ABI_I64));
    default:
        return 0;
    }
}

static const char* xl6IrCgenSyncHelper(XL6_IrRuntimeOp operation)
{
    switch ( operation ) {
    case XL6_IR_RUNTIME_MUTEX_CREATE: return "xlir_mutex_create";
    case XL6_IR_RUNTIME_MUTEX_LOCK: return "xlir_mutex_lock";
    case XL6_IR_RUNTIME_MUTEX_TRY_LOCK: return "xlir_mutex_try_lock";
    case XL6_IR_RUNTIME_MUTEX_UNLOCK: return "xlir_mutex_unlock";
    case XL6_IR_RUNTIME_SEM_CREATE: return "xlir_sem_create";
    case XL6_IR_RUNTIME_SEM_WAIT: return "xlir_sem_wait";
    case XL6_IR_RUNTIME_SEM_TRY_WAIT: return "xlir_sem_try_wait";
    case XL6_IR_RUNTIME_SEM_WAIT_TIMEOUT: return "xlir_sem_wait_timeout";
    case XL6_IR_RUNTIME_SEM_POST: return "xlir_sem_post";
    case XL6_IR_RUNTIME_SEM_POST_MULTIPLE: return "xlir_sem_post_multiple";
    case XL6_IR_RUNTIME_COND_CREATE: return "xlir_cond_create";
    case XL6_IR_RUNTIME_COND_WAIT: return "xlir_cond_wait";
    case XL6_IR_RUNTIME_COND_WAIT_TIMEOUT: return "xlir_cond_wait_timeout";
    case XL6_IR_RUNTIME_COND_SIGNAL: return "xlir_cond_signal";
    case XL6_IR_RUNTIME_COND_BROADCAST: return "xlir_cond_broadcast";
    case XL6_IR_RUNTIME_RWLOCK_CREATE: return "xlir_rwlock_create";
    case XL6_IR_RUNTIME_RWLOCK_READ_LOCK: return "xlir_rwlock_read_lock";
    case XL6_IR_RUNTIME_RWLOCK_TRY_READ_LOCK:
        return "xlir_rwlock_try_read_lock";
    case XL6_IR_RUNTIME_RWLOCK_READ_UNLOCK:
        return "xlir_rwlock_read_unlock";
    case XL6_IR_RUNTIME_RWLOCK_WRITE_LOCK: return "xlir_rwlock_write_lock";
    case XL6_IR_RUNTIME_RWLOCK_TRY_WRITE_LOCK:
        return "xlir_rwlock_try_write_lock";
    case XL6_IR_RUNTIME_RWLOCK_WRITE_UNLOCK:
        return "xlir_rwlock_write_unlock";
    case XL6_IR_RUNTIME_RWLOCK_DOWNGRADE: return "xlir_rwlock_downgrade";
    case XL6_IR_RUNTIME_RWLOCK_UPGRADE: return "xlir_rwlock_upgrade";
    case XL6_IR_RUNTIME_SEM_CREATE_DEFAULT: return "xlir_sem_create_default";
    default: return NULL;
    }
}

static int xl6IrCgenEmitSyncRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    int creates;
    const char* helper;
    const XL6_IrInstruction* receiver = NULL;
    const XL6_IrType* resourceType;
    XL6_IrValueId first = XL6_IR_VALUE_INVALID;
    if ( instruction == NULL ||
         !xl6IrCgenSyncRuntimeOpIs(instruction->runtimeOp) ) return 0;
    if ( !xl6IrCgenRuntimeSyncAbi(module, instruction) ) return -1;
    helper = xl6IrCgenSyncHelper(instruction->runtimeOp);
    if ( helper == NULL ) return -1;
    creates = xl6IrCgenSyncRuntimeOpCreates(instruction->runtimeOp);
    if ( instruction->operandCount != 0 )
        first = module->operands[instruction->firstOperand];
    if ( !creates )
        receiver = xl6IrCgenValueDefinition(module, first, NULL);
    resourceType = xl6IrTypeGet(&module->types,
        creates ? instruction->type : receiver->type);
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_SEM_CREATE )
        xl6OutputPrintf(out,
            "    v%u = %s(0x%016llXULL, v%u, v%u);\n",
            (unsigned int)instruction->result, helper,
            (unsigned long long)resourceType->typeValueId,
            (unsigned int)first,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
    else if ( creates )
        xl6OutputPrintf(out, "    v%u = %s(0x%016llXULL);\n",
            (unsigned int)instruction->result, helper,
            (unsigned long long)resourceType->typeValueId);
    else if ( instruction->runtimeOp == XL6_IR_RUNTIME_COND_WAIT ||
              instruction->runtimeOp ==
                  XL6_IR_RUNTIME_COND_WAIT_TIMEOUT ) {
        const XL6_IrInstruction* mutex = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + 1], NULL);
        const XL6_IrType* mutexType = xl6IrTypeGet(
            &module->types, mutex->type);
        xl6OutputPrintf(out,
            "    v%u = %s(v%u, 0x%016llXULL, v%u, 0x%016llXULL",
            (unsigned int)instruction->result, helper, (unsigned int)first,
            (unsigned long long)resourceType->typeValueId,
            (unsigned int)module->operands[instruction->firstOperand + 1],
            (unsigned long long)mutexType->typeValueId);
        if ( instruction->runtimeOp == XL6_IR_RUNTIME_COND_WAIT_TIMEOUT )
            xl6OutputPrintf(out, ", v%u",
                (unsigned int)module->operands[instruction->firstOperand + 2]);
        xl6OutputPuts(");\n", out);
    } else if ( instruction->operandCount == 2 )
        xl6OutputPrintf(out,
            "    v%u = %s(v%u, 0x%016llXULL, v%u);\n",
            (unsigned int)instruction->result, helper, (unsigned int)first,
            (unsigned long long)resourceType->typeValueId,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
    else
        xl6OutputPrintf(out, "    v%u = %s(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, helper, (unsigned int)first,
            (unsigned long long)resourceType->typeValueId);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenSharedRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return xl6IrCgenSharedScalarStringRuntimeOpIs(operation) ||
           xl6IrCgenSpecialF64RuntimeOpIs(operation) ||
           xl6IrCgenScalarConversionRuntimeOpIs(operation) ||
           xl6IrCgenSharedStringCompareRelation(operation) != NULL ||
           xl6IrCgenErrorProjection(operation) != NULL ||
           xl6IrCgenSharedXValueRuntimeOpIs(operation) ||
           xl6IrDynamicCompareRuntimeOpIs(operation) ||
           xl6IrCollectionMemberRuntimeOpIs(operation) ||
           xl6IrCollectionReadRuntimeOpIs(operation) ||
           xl6IrCgenThreadRuntimeOpIs(operation) ||
           xl6IrCgenChannelRuntimeOpIs(operation) ||
           xl6IrCgenSpecialQueueRuntimeOpIs(operation) ||
           xl6IrCgenStackRuntimeOpIs(operation) ||
           xl6IrCgenAvlRuntimeOpIs(operation) ||
           xl6IrCgenSyncRuntimeOpIs(operation) ||
           xl6IrCgenFutureMemberHelper(operation) != NULL;
}

static int xl6IrCgenSharedRuntimeAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    if ( instruction == NULL ) return 0;
    if ( xl6IrCgenSpecialF64RuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenRuntimeSpecialF64Abi(module, instruction);
    if ( xl6IrDynamicCompareRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrDynamicCompareRuntimeInstructionCompatible(module, instruction);
    if ( xl6IrCgenScalarConversionRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenScalarConversionRuntimeAbi(module, instruction);
    if ( xl6IrCgenSharedStringCompareRelation(instruction->runtimeOp) != NULL )
        return xl6IrCgenSharedStringCompareAbi(module, instruction);
    if ( xl6IrCgenErrorProjection(instruction->runtimeOp) != NULL )
        return xl6IrCgenErrorProjectionAbi(module, instruction);
    if ( xl6IrCgenSharedScalarStringRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenSharedScalarStringRuntimeAbi(module, instruction);
    if ( xl6IrCgenSharedXValueRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenSharedXValueRuntimeAbi(module, instruction);
    if ( xl6IrCollectionMemberRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCollectionMemberRuntimeInstructionCompatible(module, instruction);
    if ( xl6IrCollectionReadRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCollectionReadRuntimeInstructionCompatible(module, instruction);
    if ( xl6IrCgenThreadRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenRuntimeThreadAbi(module, instruction);
    if ( xl6IrCgenChannelRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenRuntimeChannelAbi(module, instruction);
    if ( xl6IrCgenSpecialQueueRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenRuntimeSpecialQueueAbi(module, instruction);
    if ( xl6IrCgenStackRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenRuntimeStackAbi(module, instruction);
    if ( xl6IrCgenAvlRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenRuntimeAvlAbi(module, instruction);
    if ( xl6IrCgenSyncRuntimeOpIs(instruction->runtimeOp) )
        return xl6IrCgenRuntimeSyncAbi(module, instruction);
    if ( xl6IrCgenFutureMemberHelper(instruction->runtimeOp) != NULL )
        return xl6IrCgenRuntimeFutureMemberAbi(module, instruction);
    return 0;
}

static int xl6IrCgenEmitSharedRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    int emitted = xl6IrCgenEmitSharedScalarStringRuntime(
        out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitSpecialF64(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitDynamicCompare(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitScalarConversionRuntime(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitSharedStringCompare(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitErrorProjection(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitSharedXValueRuntime(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitSharedCollectionRuntime(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitThreadRuntime(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitChannelRuntime(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitSpecialQueueRuntime(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitStackRuntime(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitAvlRuntime(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitSyncRuntime(out, module, instruction);
    if ( emitted == 0 )
        emitted = xl6IrCgenEmitFutureMemberRuntime(out, module, instruction);
    return emitted;
}

static int xl6IrCgenEmitLanguageFunctionValueAdapter(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_DiagList* diags)
{
    const XL6_IrType* signature = xl6IrTypeGet(&module->types, type);
    const XL6_IrType* result;
    size_t parameter;
    if ( !xl6IrCgenLanguageFunctionValueSignatureAbi(module, type) )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter XL6_Function adapter signature is unresolved");
    xl6OutputPrintf(out,
        "static xvalue* xlir_language_function_invoke_t%u(void* environment, int argc, xvalue** argv)\n"
        "{\n"
        "    xlir_closure* closure = (xlir_closure*)environment; xvalue* boxed = NULL;\n",
        (unsigned int)type);
    for ( parameter = 0; parameter < signature->paramCount; ++parameter ) {
        const XL6_IrType* item = xl6IrTypeGet(
            &module->types, signature->params[parameter]);
        const char* cType = xl6IrCgenType(module, signature->params[parameter]);
        if ( item == NULL || cType == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter XL6_Function parameter projection is unresolved");
        if ( item->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out,
                "    xlir_string_view text%lu = {0}; xlir_string a%lu = {0};\n",
                (unsigned long)parameter, (unsigned long)parameter);
        else
            xl6OutputPrintf(out, "    %s a%lu = (%s)0;\n",
                cType, (unsigned long)parameter, cType);
    }
    result = signature->callableResultType != XL6_IR_TYPE_INVALID
        ? xl6IrTypeGet(&module->types, signature->callableResultType) : NULL;
    if ( signature->callableResultType != XL6_IR_TYPE_INVALID && result == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter XL6_Function result projection is unresolved");
    if ( result != NULL ) {
        const char* cType = xl6IrCgenType(
            module, signature->callableResultType);
        if ( cType == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter XL6_Function result projection is unresolved");
        if ( result->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPuts("    xlir_string returned = {0};\n", out);
        else
            xl6OutputPrintf(out, "    %s returned = (%s)0;\n", cType, cType);
    }
    xl6OutputPrintf(out,
        "    if (closure == NULL || argc != %lu || (argc != 0 && argv == NULL)) return NULL;\n",
        (unsigned long)signature->paramCount);
    for ( parameter = 0; parameter < signature->paramCount; ++parameter ) {
        const XL6_IrType* item = xl6IrTypeGet(
            &module->types, signature->params[parameter]);
        if ( item->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR )
            xl6OutputPrintf(out,
                "    if (argv[%lu] == NULL) goto fail;\n    a%lu = argv[%lu];\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter);
        else if ( item->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out,
                "    if (argv[%lu] == NULL || !xrtValueGetString(argv[%lu], &text%lu) || (a%lu = xlir_string_from_utf8(text%lu.Data, text%lu.Size)).Data == NULL) goto fail;\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter, (unsigned long)parameter);
        else if ( item->cAbi == XL6_IR_C_ABI_RAW_POINTER )
            xl6OutputPrintf(out,
                "    if (argv[%lu] == NULL || !xrtValueGetPointer(argv[%lu], &a%lu)) goto fail;\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter);
        else if ( item->cAbi == XL6_IR_C_ABI_C_SIZE )
            xl6OutputPrintf(out,
                "    { uint64_t projected = 0; if (argv[%lu] == NULL || !xrtValueGetUInt(argv[%lu], &projected) || projected > (uint64_t)SIZE_MAX) goto fail; a%lu = (size_t)projected; }\n",
                (unsigned long)parameter, (unsigned long)parameter,
                (unsigned long)parameter);
        else {
            const char* getter = item->cAbi == XL6_IR_C_ABI_BOOL
                ? "xrtValueGetBool"
                : item->cAbi == XL6_IR_C_ABI_I64
                    ? "xrtValueGetInt"
                    : (item->cAbi == XL6_IR_C_ABI_U64 ||
                       item->cAbi == XL6_IR_C_ABI_TYPE_ID)
                        ? "xrtValueGetUInt" : "xrtValueGetFloat";
            xl6OutputPrintf(out,
                "    if (argv[%lu] == NULL || !%s(argv[%lu], &a%lu)) goto fail;\n",
                (unsigned long)parameter, getter, (unsigned long)parameter,
                (unsigned long)parameter);
        }
    }
    xl6OutputPuts("    if (!((bool (*)(void*", out);
    if ( result != NULL ) {
        const char* resultType = xl6IrCgenType(
            module, signature->callableResultType);
        xl6OutputPrintf(out, ", %s*", resultType);
    }
    for ( parameter = 0; parameter < signature->paramCount; ++parameter )
        xl6OutputPrintf(out, ", %s",
            xl6IrCgenType(module, signature->params[parameter]));
    xl6OutputPuts("))closure->invoke)(closure->environment", out);
    if ( result != NULL ) xl6OutputPuts(", &returned", out);
    for ( parameter = 0; parameter < signature->paramCount; ++parameter )
        xl6OutputPrintf(out, ", a%lu", (unsigned long)parameter);
    xl6OutputPuts(")) goto fail;\n", out);
    for ( parameter = 0; parameter < signature->paramCount; ++parameter ) {
        const XL6_IrType* item = xl6IrTypeGet(
            &module->types, signature->params[parameter]);
        if ( item->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out,
                "    xlir_string_drop(a%lu); a%lu = (xlir_string){0};\n",
                (unsigned long)parameter, (unsigned long)parameter);
    }
    if ( result == NULL )
        xl6OutputPuts("    boxed = xrtValueRetain(xrtValueNull());\n", out);
    else if ( result->cAbi == XL6_IR_C_ABI_BOOL )
        xl6OutputPuts("    boxed = xrtValueRetain(xrtValueBool(returned));\n", out);
    else if ( result->cAbi == XL6_IR_C_ABI_I64 )
        xl6OutputPuts("    boxed = xrtValueInt(returned);\n", out);
    else if ( result->cAbi == XL6_IR_C_ABI_U64 ||
              result->cAbi == XL6_IR_C_ABI_TYPE_ID )
        xl6OutputPuts("    boxed = xrtValueUInt(returned);\n", out);
    else if ( result->cAbi == XL6_IR_C_ABI_F64 )
        xl6OutputPuts("    boxed = xrtValueFloat(returned);\n", out);
    else if ( result->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
        xl6OutputPuts(
            "    if (returned.Data != NULL) boxed = xrtValueString(returned);\n"
            "    xlir_string_drop(returned); returned = (xlir_string){0};\n",
            out);
    else
        xl6OutputPuts("    boxed = returned; returned = NULL;\n", out);
    xl6OutputPuts("    return boxed;\nfail:\n", out);
    for ( parameter = 0; parameter < signature->paramCount; ++parameter ) {
        const XL6_IrType* item = xl6IrTypeGet(
            &module->types, signature->params[parameter]);
        if ( item->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
            xl6OutputPrintf(out, "    xlir_string_drop(a%lu);\n",
                (unsigned long)parameter);
    }
    if ( result != NULL && result->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
        xl6OutputPuts("    xlir_string_drop(returned);\n", out);
    else if ( result != NULL &&
              result->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR )
        xl6OutputPuts("    xrtValueRelease(returned);\n", out);
    xl6OutputPrintf(out,
        "    return NULL;\n"
        "}\n"
        "static XL6_Function xlir_language_function_value_t%u(xlir_closure* closure)\n"
        "{\n"
        "    XL6_Function value = {0};\n"
        "    if (closure != NULL) { value.env = closure; value.invoke_var = xlir_language_function_invoke_t%u; value.retain = xlir_language_function_retain; value.release = xlir_language_function_release; }\n"
        "    return value;\n"
        "}\n",
        (unsigned int)type, (unsigned int)type);
    return !xl6OutputHasFailed(out);
}

static const char* xl6IrCgenRawCallbackParameterType(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    const XL6_IrType* item = xl6IrTypeGet(&module->types, type);
    if ( item != NULL && item->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
         item->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED )
        return "const char*";
    return xl6IrCgenType(module, type);
}

static int xl6IrCgenEmitRawCallbackTypedef(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_DiagList* diags)
{
    XL6_IrTypeId signatureType = xl6IrExportCallbackSignatureType(module, type);
    const XL6_IrType* signature = xl6IrTypeGet(&module->types, signatureType);
    const char* resultType;
    size_t parameter;
    if ( signatureType == XL6_IR_TYPE_INVALID ||
         signature == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter raw C callback signature is unresolved");
    resultType = signature->callableResultType == XL6_IR_TYPE_INVALID
        ? "void" : xl6IrCgenType(module, signature->callableResultType);
    if ( resultType == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter raw C callback result ABI is unresolved");
    xl6OutputPrintf(out, "typedef %s (*xlir_raw_callback_t%u)(",
        resultType, (unsigned int)type);
    if ( signature->paramCount == 0 ) xl6OutputPuts("void", out);
    for ( parameter = 0; parameter < signature->paramCount; ++parameter ) {
        const char* parameterType = xl6IrCgenRawCallbackParameterType(
            module, signature->params[parameter]);
        if ( parameterType == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter raw C callback parameter ABI is unresolved");
        if ( parameter != 0 ) xl6OutputPuts(", ", out);
        xl6OutputPuts(parameterType, out);
    }
    xl6OutputPuts(");\n", out);
    return !xl6OutputHasFailed(out);
}

static const char* xl6IrCgenFunctionParameterType(
    const XL6_IrModule* module, const XL6_IrFunction* function, size_t parameterIndex)
{
    XL6_SymbolId symbol;
    const XL6_IrStorage* parameter;
    if ( function == NULL || parameterIndex >= function->parameterCount ) return NULL;
    if ( xl6IrCgenIsEntryParameter(module, function->symbol, parameterIndex) ) return "xvalue*";
    symbol = module->functionParameters[function->firstParameter + parameterIndex];
    /* Parameter declaration nodes are source provenance only.  Synthetic
     * monomorphic instances intentionally have no source symbol slot, so the
     * backend must consume the lowered storage ABI just like LOAD/STORE do. */
    parameter = xl6IrStorageGet(module, symbol);
    return parameter != NULL ? xl6IrCgenType(module, parameter->type) : NULL;
}

static const XL6_IrFunction* xl6IrCgenFindFunction(const XL6_IrModule* module, XL6_SymbolId symbol)
{
    size_t i;
    for ( i = 0; i < module->functionCount; ++i ) {
        if ( module->functions[i].symbol == symbol && symbol != 0 ) return &module->functions[i];
    }
    return NULL;
}

/* Lowering owns the complete callable graph.  Cgen consumes this fact and
 * never rebuilds reachability by scanning instructions or source boundaries. */
static int xl6IrCgenTargetIsReachable(const XL6_IrModule* module, XL6_SymbolId symbol)
{
    const XL6_IrCallTarget* target;
    if ( module == NULL || symbol == 0 ) return 0;
    target = xl6IrCallTargetGet(module, symbol);
    return target != NULL && target->reachability != XL6_IR_REACHABILITY_UNREACHABLE;
}

static const XL6_IrAsyncMachine* xl6IrCgenAsyncMachine(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrAsyncMachine* machine;
    const XL6_IrFunction* function;
    if ( module == NULL || target == NULL ||
         target->kind != XL6_IR_CALL_TARGET_INTERNAL_ASYNC ||
         target->asyncMachine >= module->asyncMachineCount ) return NULL;
    machine = &module->asyncMachines[target->asyncMachine];
    function = machine->function < module->functionCount
        ? &module->functions[machine->function] : NULL;
    if ( machine->status != XL6_IR_ASYNC_MACHINE_ANALYZED ||
         machine->targetSymbol != target->symbol || function == NULL ||
         function->kind != XL6_IR_FUNCTION_ASYNC_WORKER ||
         target->asyncDispatch != XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL ||
         function->symbol != target->symbol ||
         machine->suspensionCount == 0 || machine->entryCount == 0 ||
         machine->firstEntry >= module->asyncMachineEntryCount ||
         machine->entryCount > module->asyncMachineEntryCount - machine->firstEntry ||
         machine->blockCount == 0 || machine->firstBlock >= module->asyncMachineBlockCount ||
         machine->blockCount > module->asyncMachineBlockCount - machine->firstBlock ) return NULL;
    return machine;
}

static const XL6_IrGeneratorMachine* xl6IrCgenGeneratorMachine(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrGeneratorMachine* machine;
    const XL6_IrFunction* function;
    if ( module == NULL || target == NULL ||
         target->kind != XL6_IR_CALL_TARGET_INTERNAL_GENERATOR ||
         target->generatorMachine >= module->generatorMachineCount ) return NULL;
    machine = &module->generatorMachines[target->generatorMachine];
    function = machine->function < module->functionCount
        ? &module->functions[machine->function] : NULL;
    if ( machine->status != XL6_IR_GENERATOR_MACHINE_ANALYZED ||
         machine->targetSymbol != target->symbol || function == NULL ||
         function->kind != XL6_IR_FUNCTION_GENERATOR_WORKER ||
         function->symbol != target->symbol || machine->entryCount == 0 ||
         machine->firstEntry >= module->generatorMachineEntryCount ||
         machine->entryCount >
            module->generatorMachineEntryCount - machine->firstEntry ||
         machine->blockCount == 0 ||
         machine->firstBlock >= module->generatorMachineBlockCount ||
         machine->blockCount >
            module->generatorMachineBlockCount - machine->firstBlock ) return NULL;
    return machine;
}

/* A function linked to an analyzed async machine is the machine's canonical
 * program source, not a second ordinary C callable.  Keep this decision tied
 * to the verified function/target/machine links so Cgen never infers it from
 * AWAIT syntax, reachability or the former compatibility dispatch role. */
static int xl6IrCgenFunctionIsCanonicalMachineProgram(
    const XL6_IrModule* module, const XL6_IrFunction* function)
{
    const XL6_IrCallTarget* target;
    const XL6_IrAsyncMachine* machine;
    const XL6_IrGeneratorMachine* generator;
    if ( module == NULL || function == NULL || function->symbol == 0 ||
         function < module->functions ||
         function >= module->functions + module->functionCount ) return 0;
    target = xl6IrCallTargetGet(module, function->symbol);
    machine = xl6IrCgenAsyncMachine(module, target);
    if ( machine != NULL &&
         machine->function == (size_t)(function - module->functions) ) return 1;
    generator = xl6IrCgenGeneratorMachine(module, target);
    return generator != NULL &&
           generator->function == (size_t)(function - module->functions);
}
