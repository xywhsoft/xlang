static int xl6IrCgenEmitChannelRuntime(XL6_Output*,const XL6_IrModule*,const XL6_IrInstruction*);

static int xl6IrCgenRuntimeSpecialQueueAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    if (xl6IrConcurrentQueueInstructionType(module,instruction))
        return xl6IrConcurrentQueueInstructionCompatible(module,instruction);
    const XL6_IrInstruction* receiver = NULL;
    const XL6_IrInstruction* payloadValue = NULL;
    const XL6_IrInstruction* timeoutValue = NULL;
    const XL6_IrType* queue = NULL;
    const XL6_IrType* payload = NULL;
    const XL6_IrType* resultType;
    XL6_IrTypeId queueTypeId;
    int creates;
    int sends;
    int receives;
    int takesTimeout;
    int closes;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenSpecialQueueRuntimeOpIs(instruction->runtimeOp) ||
         instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount >
             module->operandCount - instruction->firstOperand ) return 0;
    creates = instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE;
    sends = instruction->runtimeOp ==
        XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_PUSH;
    receives = instruction->runtimeOp ==
            XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_POP_INTO ||
        instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_INTO ||
        instruction->runtimeOp ==
            XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_TIMEOUT_INTO;
    takesTimeout = instruction->runtimeOp ==
        XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_TIMEOUT_INTO;
    closes = instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_CLOSE;
    if ( !creates && instruction->operandCount != 0 )
        receiver = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand], NULL);
    queueTypeId = creates ? instruction->type
                          : receiver != NULL ? receiver->type
                                             : XL6_IR_TYPE_INVALID;
    if ( !xl6IrCgenSpecialQueueTypeAbi(
             module, queueTypeId, &queue, &payload) ||
         queue->runtimeResourceAbi != XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE ||
         instruction->payloadType != queue->params[0] ||
         instruction->runtimeResourcePayloadAbi !=
             xl6IrCgenChannelPayloadAbi(payload) ) return 0;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    if ( creates ) {
        XL6_IrNominalIdentitySetId admissionSet=XL6_IR_NOMINAL_IDENTITY_SET_INVALID;
        if (instruction->sequenceAdmissionIdentitySet!=admissionSet) return 0;
        const XL6_IrInstruction* capacity = instruction->operandCount == 1
            ? xl6IrCgenValueDefinition(
                module, module->operands[instruction->firstOperand], NULL)
            : NULL;
        const XL6_IrType* capacityType = capacity != NULL
            ? xl6IrTypeGet(&module->types, capacity->type) : NULL;
        return instruction->result != XL6_IR_VALUE_INVALID &&
               instruction->operandCount == 1 && capacityType != NULL &&
               capacityType->cAbi == XL6_IR_C_ABI_I64 &&
               capacityType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
               capacityType->integerTarget ==
                   XL6_IR_INTEGER_TARGET_SIGNED_64;
    }
    if ( receiver == NULL || receiver->type != queueTypeId ) return 0;
    if ( closes )
        return instruction->operandCount == 1 &&
               instruction->result == XL6_IR_VALUE_INVALID &&
               instruction->type == XL6_IR_TYPE_INVALID;
    if ( instruction->result == XL6_IR_VALUE_INVALID || resultType == NULL )
        return 0;
    if ( sends ) {
        payloadValue = instruction->operandCount == 2
            ? xl6IrCgenValueDefinition(
                module, module->operands[instruction->firstOperand + 1], NULL)
            : NULL;
        return payloadValue != NULL &&
               payloadValue->type == instruction->payloadType &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    }
    if ( receives ) {
        const XL6_IrType* referenceType;
        payloadValue = instruction->operandCount >= 2
            ? xl6IrCgenValueDefinition(
                module, module->operands[instruction->firstOperand + 1], NULL)
            : NULL;
        timeoutValue = instruction->operandCount == 3
            ? xl6IrCgenValueDefinition(
                module, module->operands[instruction->firstOperand + 2], NULL)
            : NULL;
        referenceType = payloadValue != NULL
            ? xl6IrTypeGet(&module->types, payloadValue->type) : NULL;
        return instruction->operandCount == (takesTimeout ? 3u : 2u) &&
               payloadValue != NULL &&
               payloadValue->kind == XL6_IR_INSTR_STORAGE_ADDRESS &&
               referenceType != NULL &&
               referenceType->kind == XL6_IR_TYPE_STORAGE_REF &&
               referenceType->cAbi == XL6_IR_C_ABI_STORAGE_REF_PTR &&
               referenceType->paramCount == 1 &&
               referenceType->params != NULL &&
               referenceType->params[0] == instruction->payloadType &&
               (!takesTimeout ||
                (timeoutValue != NULL &&
                 xl6IrTypeGet(&module->types, timeoutValue->type) != NULL &&
                 xl6IrTypeGet(&module->types, timeoutValue->type)->cAbi ==
                     XL6_IR_C_ABI_I64 &&
                 xl6IrTypeGet(&module->types, timeoutValue->type)->ownershipAbi ==
                     XL6_IR_OWNERSHIP_ABI_TRIVIAL)) &&
               ((instruction->runtimeOp !=
                      XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_INTO &&
                 !takesTimeout) ||
                queue->runtimeResourceAbi ==
                    XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE) &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    }
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_COUNT ||
         instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_CAPACITY )
        return instruction->operandCount == 1 &&
               resultType->cAbi == XL6_IR_C_ABI_I64 &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
               resultType->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_64;
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_RESET )
        return queue->runtimeResourceAbi !=
                   XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE &&
               instruction->operandCount == 1 &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    if ( instruction->runtimeOp ==
             XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_CLOSED ||
         instruction->runtimeOp ==
             XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED )
        return instruction->operandCount == 1 &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    return 0;
}

static int xl6IrCgenEmitConcurrentQueueRuntime(XL6_Output*,const XL6_IrModule*,const XL6_IrInstruction*);
static int xl6IrCgenEmitSpecialQueueRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if (xl6IrConcurrentQueueInstructionType(module,instruction))
        return xl6IrCgenEmitConcurrentQueueRuntime(out,module,instruction);
    const XL6_IrInstruction* receiver;
    const XL6_IrType* queue;
    const XL6_IrType* payload;
    const char* payloadType;
    const char* payloadAbi;
    XL6_IrValueId first;
    if ( instruction == NULL ||
         !xl6IrCgenSpecialQueueRuntimeOpIs(instruction->runtimeOp) ) return 0;
    if ( !xl6IrCgenRuntimeSpecialQueueAbi(module, instruction) ) return -1;
    first = instruction->operandCount != 0
        ? module->operands[instruction->firstOperand] : XL6_IR_VALUE_INVALID;
    receiver = instruction->runtimeOp == XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE
        ? NULL : xl6IrCgenValueDefinition(module, first, NULL);
    if ( !xl6IrCgenSpecialQueueTypeAbi(
             module, receiver != NULL ? receiver->type : instruction->type,
             &queue, &payload) ) return -1;
    payloadType = xl6IrCgenType(module, instruction->payloadType);
    payloadAbi = xl6IrCgenChannelPayloadAbiToken(
        instruction->runtimeResourcePayloadAbi);
    if ( payloadType == NULL || payloadAbi == NULL ) return -1;
#define XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS                              \
    (unsigned long long)queue->typeValueId,                                \
    (unsigned long long)payload->typeValueId, payloadAbi, payloadType
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE:
        xl6OutputPrintf(out,
            "    v%u = xlir_special_queue_create(0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), v%u);\n",
            (unsigned int)instruction->result,
            XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS, (unsigned int)first);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_PUSH:
        xl6OutputPrintf(out,
            "    v%u = xlir_special_queue_try_push(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), &v%u);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_POP_INTO:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_INTO:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_TIMEOUT_INTO:
        xl6OutputPrintf(out,
            "    v%u = xlir_special_queue_pop(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), v%u, %s, ",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS,
            (unsigned int)module->operands[instruction->firstOperand + 1],
            instruction->runtimeOp ==
                    XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_POP_INTO
                ? "XLIR_CHANNEL_RECV_TRY"
                : instruction->runtimeOp ==
                      XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_INTO
                    ? "XLIR_CHANNEL_RECV_BLOCK"
                    : "XLIR_CHANNEL_RECV_TIMEOUT");
        if ( instruction->runtimeOp ==
                 XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_TIMEOUT_INTO )
            xl6OutputPrintf(out, "v%u);\n",
                (unsigned int)module->operands[
                    instruction->firstOperand + 2]);
        else xl6OutputPuts("0);\n", out);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CLOSE:
        xl6OutputPrintf(out,
            "    xlir_special_queue_close(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s));\n",
            (unsigned int)first, XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_COUNT:
        xl6OutputPrintf(out,
            "    v%u = xlir_special_queue_count(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s));\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CAPACITY:
        xl6OutputPrintf(out,
            "    v%u = xlir_special_queue_capacity(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s));\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_RESET:
        xl6OutputPrintf(out,
            "    v%u = xlir_special_queue_reset(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s));\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_CLOSED:
        xl6OutputPrintf(out,
            "    v%u = xlir_special_queue_is_closed(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s));\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS);
        break;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED:
        xl6OutputPrintf(out,
            "    v%u = xlir_special_queue_is_drained(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s));\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS);
        break;
    default:
#undef XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS
        return 0;
    }
#undef XL6_IR_CGEN_SPECIAL_QUEUE_COMMON_ARGS
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenRuntimeStackAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* receiver = NULL;
    const XL6_IrInstruction* second = NULL;
    const XL6_IrType* stack = NULL;
    const XL6_IrType* payload = NULL;
    const XL6_IrType* resultType;
    XL6_IrTypeId stackTypeId;
    int creates;
    if (instruction && instruction->runtimeOp>=XL6_IR_RUNTIME_STACK_PUSH_BATCH &&
        instruction->runtimeOp<=XL6_IR_RUNTIME_STACK_PEEK_BATCH_AT)
        return xl6IrStackBatchInstructionCompatible(module,instruction);
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenStackRuntimeOpIs(instruction->runtimeOp) ||
         instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount >
             module->operandCount - instruction->firstOperand ) return 0;
    creates = instruction->runtimeOp == XL6_IR_RUNTIME_STACK_CREATE;
    if ( !creates && instruction->operandCount != 0 )
        receiver = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand], NULL);
    stackTypeId = creates ? instruction->type
                          : receiver != NULL ? receiver->type
                                             : XL6_IR_TYPE_INVALID;
    if ( !xl6IrCgenStackTypeAbi(
             module, stackTypeId, &stack, &payload) ||
         instruction->payloadType != stack->params[0] ||
         instruction->runtimeResourcePayloadAbi !=
             xl6IrCgenChannelPayloadAbi(payload) ) return 0;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    if ( creates )
        return instruction->operandCount == 0 &&
               instruction->result != XL6_IR_VALUE_INVALID;
    if ( receiver == NULL || receiver->type != stackTypeId ) return 0;
    if ( instruction->result == XL6_IR_VALUE_INVALID || resultType == NULL )
        return 0;
    if ( instruction->operandCount > 1 )
        second = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + 1], NULL);
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_STACK_PUSH:
        return instruction->operandCount == 2 && second != NULL &&
               second->type == instruction->payloadType &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    case XL6_IR_RUNTIME_STACK_RESERVE:
        return instruction->operandCount == 2 && second != NULL &&
               xl6IrTypeGet(&module->types, second->type) != NULL &&
               xl6IrTypeGet(&module->types, second->type)->cAbi ==
                   XL6_IR_C_ABI_I64 &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    case XL6_IR_RUNTIME_STACK_CLONE:
        return instruction->operandCount == 1 &&
               instruction->type == stackTypeId;
    case XL6_IR_RUNTIME_STACK_POP:
    case XL6_IR_RUNTIME_STACK_PEEK:
        return instruction->operandCount == 1 && instruction->type == instruction->payloadType;
    case XL6_IR_RUNTIME_STACK_PEEK_AT:
        return instruction->operandCount == 2 && instruction->type == instruction->payloadType &&
               second != NULL && xl6IrTypeGet(&module->types, second->type) != NULL &&
               xl6IrTypeGet(&module->types, second->type)->cAbi == XL6_IR_C_ABI_I64;
    case XL6_IR_RUNTIME_STACK_TRY_POP:
    case XL6_IR_RUNTIME_STACK_TRY_PEEK:
        return instruction->operandCount == 1 &&
               xl6IrStackTryResultCompatible(module, instruction->type, instruction->payloadType);
    case XL6_IR_RUNTIME_STACK_TRY_PEEK_AT:
        return instruction->operandCount == 2 && second != NULL &&
               xl6IrTypeGet(&module->types, second->type) != NULL &&
               xl6IrTypeGet(&module->types, second->type)->cAbi == XL6_IR_C_ABI_I64 &&
               xl6IrStackTryResultCompatible(module, instruction->type, instruction->payloadType);
    case XL6_IR_RUNTIME_STACK_EQUALS:
        return instruction->operandCount == 2 && second != NULL &&
               second->type == stackTypeId &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    case XL6_IR_RUNTIME_STACK_TRIM:
    case XL6_IR_RUNTIME_STACK_CLEAR:
        return instruction->operandCount == 1 &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    case XL6_IR_RUNTIME_STACK_COUNT:
    case XL6_IR_RUNTIME_STACK_CAPACITY:
        return instruction->operandCount == 1 &&
               resultType->cAbi == XL6_IR_C_ABI_I64 &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
               resultType->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_64;
    default:
        return 0;
    }
}

static int xl6IrCgenStorageRefTo(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction,
    XL6_IrTypeId targetType)
{
    const XL6_IrType* reference = instruction != NULL
        ? xl6IrTypeGet(&module->types, instruction->type) : NULL;
    return instruction != NULL &&
           instruction->kind == XL6_IR_INSTR_STORAGE_ADDRESS &&
           reference != NULL &&
           reference->kind == XL6_IR_TYPE_STORAGE_REF &&
           reference->cAbi == XL6_IR_C_ABI_STORAGE_REF_PTR &&
           reference->paramCount == 1 && reference->params != NULL &&
           reference->params[0] == targetType;
}

static int xl6IrCgenRuntimeAvlAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* operands[4] = { NULL, NULL, NULL, NULL };
    const XL6_IrType* tree = NULL;
    const XL6_IrType* key = NULL;
    const XL6_IrType* value = NULL;
    const XL6_IrType* resultType;
    XL6_IrTypeId treeTypeId;
    size_t i;
    int creates;
    if(instruction && xl6IrTreeQuerySpecRuntime(instruction->runtimeOp))
        return xl6IrTreeQueryInstructionCompatible(module,instruction);
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenAvlRuntimeOpIs(instruction->runtimeOp) ||
         instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount >
             module->operandCount - instruction->firstOperand ||
         instruction->operandCount > 4 ) return 0;
    for ( i = 0; i < instruction->operandCount; ++i )
        operands[i] = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + i], NULL);
    creates = instruction->runtimeOp == XL6_IR_RUNTIME_AVL_TREE_CREATE;
    treeTypeId = creates ? instruction->type
        : operands[0] != NULL ? operands[0]->type : XL6_IR_TYPE_INVALID;
    if ( !xl6IrCgenAvlTypeAbi(
             module, treeTypeId, &tree, &key, &value) ||
         instruction->payloadType != tree->params[1] ||
         instruction->runtimeResourcePayloadAbi !=
             xl6IrCgenChannelPayloadAbi(value) ||
         instruction->runtimeResourceKeyType != tree->params[0] ||
         instruction->runtimeResourceKeyPayloadAbi !=
             xl6IrCgenChannelPayloadAbi(key) ||
         instruction->runtimeResourceKeyOrderAbi !=
             xl6IrAvlKeyOrderAbi(module,treeTypeId) ) return 0;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    if ( creates )
        return instruction->operandCount == 0 &&
               instruction->result != XL6_IR_VALUE_INVALID;
    if ( operands[0] == NULL || operands[0]->type != treeTypeId ) return 0;
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_AVL_TREE_CLEAR )
        return instruction->operandCount == 1 &&
               instruction->result != XL6_IR_VALUE_INVALID && resultType != NULL &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL;
    if ( instruction->result == XL6_IR_VALUE_INVALID || resultType == NULL )
        return 0;
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_AVL_TREE_SET:
        return instruction->operandCount == 3 && operands[1] != NULL &&
               operands[1]->type == tree->params[0] && operands[2] != NULL &&
               operands[2]->type == tree->params[1] &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL;
    case XL6_IR_RUNTIME_AVL_TREE_CONTAINS:
    case XL6_IR_RUNTIME_AVL_TREE_REMOVE:
        return instruction->operandCount == 2 && operands[1] != NULL &&
               operands[1]->type == tree->params[0] &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL;
    case XL6_IR_RUNTIME_AVL_TREE_TRIM_TO:
        return instruction->operandCount == 2 && operands[1] != NULL &&
               xl6IrTypeGet(&module->types, operands[1]->type) != NULL &&
               xl6IrTypeGet(&module->types, operands[1]->type)->cAbi ==
                   XL6_IR_C_ABI_I64 &&
               resultType->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_64;
    case XL6_IR_RUNTIME_AVL_TREE_EQUALS:
        return instruction->operandCount == 2 && operands[1] != NULL &&
               operands[1]->type == treeTypeId &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL;
    case XL6_IR_RUNTIME_AVL_TREE_MERGE:
        return instruction->operandCount == 3 && operands[1] != NULL &&
               operands[1]->type == treeTypeId && operands[2] != NULL &&
               xl6IrTypeGet(&module->types, operands[2]->type) != NULL &&
               xl6IrTypeGet(&module->types, operands[2]->type)->cAbi ==
                   XL6_IR_C_ABI_BOOL &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL;
    case XL6_IR_RUNTIME_AVL_TREE_COUNT:
    case XL6_IR_RUNTIME_AVL_TREE_TRIM:
        return instruction->operandCount == 1 &&
               resultType->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_64;
    case XL6_IR_RUNTIME_AVL_TREE_KEYS:
    case XL6_IR_RUNTIME_AVL_TREE_VALUES:
        return instruction->operandCount == 1 &&
               xl6IrAvlProjectionContract(module,treeTypeId,instruction->type,
                   instruction->runtimeOp==XL6_IR_RUNTIME_AVL_TREE_KEYS,NULL) &&
               resultType->kind == XL6_IR_TYPE_GENERIC_INSTANCE &&
               resultType->collectionAbi == XL6_IR_COLLECTION_ARRAY &&
               resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
               resultType->typeValueId != 0 && resultType->paramCount == 1 &&
               resultType->params != NULL && resultType->params[0] ==
                   (instruction->runtimeOp == XL6_IR_RUNTIME_AVL_TREE_KEYS
                       ? tree->params[0] : tree->params[1]);
    case XL6_IR_RUNTIME_AVL_TREE_CLONE:
        return instruction->operandCount == 1 &&
               instruction->type == treeTypeId;
    default:
        return 0;
    }
}

/* Shared mechanical ARRAY/STACK factory selection. A target recipe does not
 * authorize source carrier cutover: the caller still verifies its producer.
 * No element traversal, schema reconstruction or generated codec is emitted. */
static const char* xl6IrCgenSequenceTransferAbiToken(XL6_IrRuntimeResourcePayloadAbi transfer)
{
    /* Sequence is not a Channel prelude consumer. Emit the verified private
     * transfer ABI directly, without an unrelated generated macro dependency. */
    switch (transfer) {
    case XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY: return "1";
    case XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY: return "2";
    case XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN: return "3";
    case XL6_IR_RUNTIME_RESOURCE_PAYLOAD_FUTURE_REF: return "4";
    case XL6_IR_RUNTIME_RESOURCE_PAYLOAD_NATIVE_VALUE_COPY: return "5";
    default: return NULL;
    }
}
typedef struct XL6_IrCgenValueCollectionTarget {
    const char* layout;
    const char* create;
    const char* project;
    const char* schema;
    const char* importProvider;
} XL6_IrCgenValueCollectionTarget;
/* Closed declarative target table; an unknown family never means Set. */
static const XL6_IrCgenValueCollectionTarget* xl6IrCgenValueCollectionTarget(XL6_IrCollectionAbi family)
{
    static const XL6_IrCgenValueCollectionTarget list={"XL6_ListLayoutV1","xl6ListCreateTakeV1",
        "xl6ListProjectionTakeV1","xl6SequenceListCreateTakeV1","xl6ListImportProviderV1"};
    static const XL6_IrCgenValueCollectionTarget set={"XL6_SetLayoutV1","xl6SetCreateTakeV1",
        "xl6SetProjectionTakeV1","xl6SequenceSetCreateTakeV1","xl6SetImportProviderV1"};
    static const XL6_IrCgenValueCollectionTarget dict={"XL6_DictLayoutV1","xl6DictCreateTakeV1",
        "xl6DictProjectionTakeV1","xl6SequenceDictCreateTakeV1","xl6DictImportProviderV1"};
    switch (family) {
    case XL6_IR_COLLECTION_LIST: return &list;
    case XL6_IR_COLLECTION_SET: return &set;
    case XL6_IR_COLLECTION_DICT: return &dict;
    default: return NULL;
    }
}
static int xl6IrCgenEmitImportProviders(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrTypeId* types,size_t count)
{
    for (size_t i=0;i<count;++i) {
        const XL6_IrType* type=xl6IrTypeGet(&module->types,types[i]);
        const XL6_IrCgenValueCollectionTarget* target=type ? xl6IrCgenValueCollectionTarget(type->collectionAbi) : NULL;
        if (!type || (!target && type->collectionAbi!=XL6_IR_COLLECTION_ARRAY)) return 0;
        if (i) xl6OutputPuts(",",out);
        if (target) xl6OutputPrintf(out,"%s()",target->importProvider);
        else xl6OutputPuts("NULL",out);
    }
    return !xl6OutputHasFailed(out);
}
static XL6_IrCollectionAbi xl6IrCgenValueCollectionFamily(XL6_IrSequenceLayoutAbi abi)
{
    switch (abi) {
    case XL6_IR_SEQUENCE_LAYOUT_LIST_REFERENCE_V1: return XL6_IR_COLLECTION_LIST;
    case XL6_IR_SEQUENCE_LAYOUT_SET_REFERENCE_V1: return XL6_IR_COLLECTION_SET;
    case XL6_IR_SEQUENCE_LAYOUT_DICT_REFERENCE_V1: return XL6_IR_COLLECTION_DICT;
    default: return XL6_IR_COLLECTION_NONE;
    }
}
static int xl6IrCgenEmitTreeReferenceCreateDepth(XL6_Output*,const XL6_IrModule*,
    XL6_IrTypeId,const char*,size_t);
static int xl6IrCgenEmitQueueReferenceCreateDepth(XL6_Output*,const XL6_IrModule*,
    XL6_IrTypeId,const char*,size_t);
static int xl6IrCgenEmitListTargetCreateDepth(XL6_Output*,const XL6_IrModule*,
    XL6_IrTypeId,const char*,XL6_IrNominalIdentitySetId,size_t,const char*,size_t);
static int xl6IrCgenEmitValueCollectionTargetCreateDepth(XL6_Output*,const XL6_IrModule*,
    XL6_IrTypeId,XL6_IrCollectionAbi,const char*,XL6_IrNominalIdentitySetId,size_t,const char*,size_t);
static int xl6IrCgenEmitSequenceCreateDepth(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId typeId,const char* result,XL6_IrNominalIdentitySetId admissionSet,size_t depth,
    const char* importChain,size_t importCount,bool projectArrays,bool captureSeed)
{
    const XL6_IrType* sequence=xl6IrTypeGet(&module->types,typeId);
    XL6_IrNominalIdentitySetId expectedSet=XL6_IR_NOMINAL_IDENTITY_SET_INVALID;
    if (depth>64 || !sequence || !xl6IrSequenceConstructionRecipe(module,typeId,sequence->sequenceLayout.role,&expectedSet) ||
        admissionSet!=expectedSet) return -1;
    const XL6_IrSequenceLayout* plan=&sequence->sequenceLayout;
    bool any=plan->abi==XL6_IR_SEQUENCE_LAYOUT_DYNAMIC_VALUE;
    XL6_IrTypeId element=any ? xl6IrArrayAnyElementType(module,typeId) : plan->elementType;
    const XL6_IrType* payload=xl6IrTypeGet(&module->types,element);
    const XL6_IrType* base=any ? payload : xl6IrTypeGet(&module->types,plan->baseElementType);
    const char* descriptor=any ? "xrtTypeValue()" : xl6IrCgenNativeSlotTypeExpr(plan->slotAbi);
    const char* baseDescriptor=xl6IrCgenNativeValueTypeExpr(module,plan->baseElementType);
    const char* payloadType=plan->codecAbi==XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1 ||
        plan->codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1 ? "xvalue*" : xl6IrCgenType(module,element);
    const char* transfer=any ? "3" : xl6IrCgenSequenceTransferAbiToken(plan->transferAbi);
#define XL6_SEQUENCE_CREATE_IDS result,(unsigned)plan->role, \
    (unsigned long long)sequence->typeValueId,(unsigned long long)payload->typeValueId
    switch (plan->abi) {
    case XL6_IR_SEQUENCE_LAYOUT_UNION_VALUE_V1:
        xl6OutputPrintf(out,
            "    %s = xl6SequenceUnionCreateV1(%u, 0x%016llXULL, 0x%016llXULL, 0x%016llXULL, sizeof(%s), %s);\n",
            XL6_SEQUENCE_CREATE_IDS,(unsigned long long)base->typeValueId,
            xl6IrCgenType(module,plan->baseElementType),plan->nullable ? "true" : "false");
        break;
    case XL6_IR_SEQUENCE_LAYOUT_NATIVE_VALUE_V1:
        {
            const XL6_IrNativeValuePlan* native=xl6IrNativeValuePlanForType(payload);
            if (!native || plan->nullable || plan->slotAbi!=native->slotAbi) return -1;
            xl6OutputPrintf(out,"    %s = %s(%u, 0x%016llXULL);\n",
                result,native->sequenceSymbol,(unsigned)plan->role,(unsigned long long)sequence->typeValueId);
        }
        break;
    case XL6_IR_SEQUENCE_LAYOUT_DYNAMIC_VALUE:
        xl6OutputPrintf(out,
            "    %s = xl6SequenceReferenceCreateV1(%u, 0x%016llXULL, 0x%016llXULL, %u, false, 0x%016llXULL);\n",
            XL6_SEQUENCE_CREATE_IDS,(unsigned)XL6_VALUE_ADMIT_ANY,(unsigned long long)base->typeValueId);
        break;
    case XL6_IR_SEQUENCE_LAYOUT_PRIMITIVE_V2:
        if (!descriptor || !payloadType || !transfer) return -1;
        xl6OutputPrintf(out,
            "    %s = xl6SequencePrimitiveCreateV1(%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, %u, %s, 0x%016llXULL);\n",
            XL6_SEQUENCE_CREATE_IDS,transfer,payloadType,descriptor,(unsigned)plan->admissionAbi,
            plan->nullable ? "true" : "false",(unsigned long long)base->typeValueId);
        break;
    case XL6_IR_SEQUENCE_LAYOUT_REFERENCE_V2:
        xl6OutputPrintf(out,
            "    %s = xl6SequenceReferenceCreateV1(%u, 0x%016llXULL, 0x%016llXULL, %u, %s, 0x%016llXULL);\n",
            XL6_SEQUENCE_CREATE_IDS,(unsigned)plan->admissionAbi,plan->nullable ? "true" : "false",
            (unsigned long long)base->typeValueId);
        break;
    case XL6_IR_SEQUENCE_LAYOUT_NOMINAL_V2:
        xl6OutputPrintf(out,
            "    %s = xl6SequenceNominalCreateV1(%u, 0x%016llXULL, 0x%016llXULL, &xlir_nominal_set_%u, %s, 0x%016llXULL);\n",
            XL6_SEQUENCE_CREATE_IDS,(unsigned)admissionSet,plan->nullable ? "true" : "false",
            (unsigned long long)base->typeValueId);
        break;
    case XL6_IR_SEQUENCE_LAYOUT_C_SCALAR_V1:
        if (!descriptor) return -1;
        xl6OutputPrintf(out,
            "    %s = xl6SequenceCScalarCreateV1(%u, 0x%016llXULL, 0x%016llXULL, %s, 0x%016llXULL);\n",
            XL6_SEQUENCE_CREATE_IDS,descriptor,(unsigned long long)base->typeValueId);
        break;
    case XL6_IR_SEQUENCE_LAYOUT_NULLABLE_C_SCALAR_V1:
        if (!descriptor || !baseDescriptor) return -1;
        xl6OutputPrintf(out,
            "    %s = xl6SequenceNullableCScalarCreateV1(%u, 0x%016llXULL, 0x%016llXULL, %s, %s, 0x%016llXULL);\n",
            XL6_SEQUENCE_CREATE_IDS,descriptor,baseDescriptor,(unsigned long long)base->typeValueId);
        break;
    case XL6_IR_SEQUENCE_LAYOUT_NUMERIC_ENUM_V1:
        if (!descriptor) return -1;
        xl6OutputPrintf(out,
            "    %s = xl6SequenceNumericEnumCreateV1(%u, 0x%016llXULL, 0x%016llXULL, %s, %u, %s, 0x%016llXULL);\n",
            XL6_SEQUENCE_CREATE_IDS,descriptor,(unsigned)plan->admissionAbi,
            plan->nullable ? "true" : "false",(unsigned long long)base->typeValueId);
        break;
    case XL6_IR_SEQUENCE_LAYOUT_MUTEX_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_COND_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_RWLOCK_REFERENCE_V1:
        xl6OutputPrintf(out,
            "    %s = xl6SequenceSyncCreateV1(%u, 0x%016llXULL, 0x%016llXULL, 0x%016llXULL, %s, %u);\n",
            result,(unsigned)plan->role,(unsigned long long)sequence->typeValueId,
            (unsigned long long)payload->typeValueId,(unsigned long long)base->typeValueId,
            plan->nullable ? "true" : "false",
            plan->abi==XL6_IR_SEQUENCE_LAYOUT_MUTEX_REFERENCE_V1 ? 1u :
            plan->abi==XL6_IR_SEQUENCE_LAYOUT_COND_REFERENCE_V1 ? 2u : 3u);
        break;
    case XL6_IR_SEQUENCE_LAYOUT_CALLABLE_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_FUTURE_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_SEMAPHORE_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_THREAD_REFERENCE_V1:
        xl6OutputPrintf(out,
            "    %s = %s(%u, 0x%016llXULL, 0x%016llXULL, 0x%016llXULL, %s);\n",
            result,plan->abi==XL6_IR_SEQUENCE_LAYOUT_CALLABLE_REFERENCE_V1 ?
                "xl6SequenceCallableCreateV1" :
                plan->abi==XL6_IR_SEQUENCE_LAYOUT_SEMAPHORE_REFERENCE_V1 ?
                "xl6SequenceSemCreateV1" :
                plan->abi==XL6_IR_SEQUENCE_LAYOUT_THREAD_REFERENCE_V1 ?
                "xl6SequenceThreadCreateV1" : "xl6SequenceFutureCreateV1",
            (unsigned)plan->role,(unsigned long long)sequence->typeValueId,
            (unsigned long long)payload->typeValueId,(unsigned long long)base->typeValueId,
            plan->nullable ? "true" : "false");
        break;
    case XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1:
        {
            XL6_IrNominalIdentitySetId childSet=XL6_IR_NOMINAL_IDENTITY_SET_INVALID;
            char childResult[48];
            if (!xl6IrSequenceConstructionRecipe(module,plan->baseElementType,plan->childRole,&childSet)) return -1;
            snprintf(childResult,sizeof(childResult),"xlir_sequence_schema_%u",(unsigned)(depth+1));
            /* Render the already-verified child recipe, not a layout-only
             * approximation. The resident parent consumes the temporary seed
             * on either path and retains only its immutable owned schema. */
            xl6OutputPrintf(out,"    { xvalue* %s = NULL;\n",childResult);
            if (xl6IrCgenEmitSequenceCreateDepth(out,module,plan->baseElementType,
                childResult,childSet,depth+1,importChain,importCount,projectArrays,true)<0) return -1;
            xl6OutputPrintf(out,
                "    %s = xl6SequenceNestedCreateTakeV2(%u, 0x%016llXULL, 0x%016llXULL, %s, %s); }\n",
                XL6_SEQUENCE_CREATE_IDS,childResult,plan->nullable ? "true" : "false");
        }
        break;
    case XL6_IR_SEQUENCE_LAYOUT_LIST_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_SET_REFERENCE_V1:
    case XL6_IR_SEQUENCE_LAYOUT_DICT_REFERENCE_V1:
        {
            XL6_IrValueCollectionRecipe child={0};
            XL6_IrCollectionAbi family=xl6IrCgenValueCollectionFamily(plan->abi);
            const XL6_IrCgenValueCollectionTarget* target=xl6IrCgenValueCollectionTarget(family);
            char childResult[48];
            if (!target || !xl6IrValueCollectionConstructionRecipe(module,plan->baseElementType,&child) || child.family!=family) return -1;
            snprintf(childResult,sizeof(childResult),"xlir_collection_schema_%u",(unsigned)(depth+1));
            xl6OutputPrintf(out,"    { xvalue* %s = NULL;\n",childResult);
            if (xl6IrCgenEmitValueCollectionTargetCreateDepth(out,module,plan->baseElementType,
                family,childResult,child.admissionIdentitySet,depth+1,importChain,importCount)<0) return -1;
            xl6OutputPrintf(out,
                "    %s = %s(%u, 0x%016llXULL, 0x%016llXULL, %s, %s); }\n",
                result,target->schema,
                (unsigned)plan->role,(unsigned long long)sequence->typeValueId,
                (unsigned long long)payload->typeValueId,childResult,plan->nullable ? "true" : "false");
        }
        break;
    case XL6_IR_SEQUENCE_LAYOUT_TREE_REFERENCE_V1:
        if (xl6IrCgenEmitTreeReferenceCreateDepth(out,module,typeId,result,depth+1)<0) return -1;
        break;
    case XL6_IR_SEQUENCE_LAYOUT_QUEUE_REFERENCE_V1:
        if (xl6IrCgenEmitQueueReferenceCreateDepth(out,module,typeId,result,depth+1)<0) return -1;
        break;
    default: return -1;
    }
#undef XL6_SEQUENCE_CREATE_IDS
    if (projectArrays && plan->role==XL6_IR_SEQUENCE_ROLE_ARRAY) {
        xl6OutputPrintf(out,
            "    %s = xl6ArrayProjectionTakeV1(%s, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, %s);\n",
            result,result,(unsigned long long)sequence->typeValueId,
            (unsigned long long)payload->typeValueId,transfer,payloadType,descriptor,
            any || plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN ? "NULL" :
                xl6IrCgenNativeValueConverterExpr(module,plan->elementType));
    }
    if (captureSeed && importChain && depth<importCount) {
        xl6OutputPrintf(out,
            "    %s[%u] = %s ? xl6ArrayEraseV1(%s, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, %s) : NULL;\n",
            importChain,(unsigned)depth,result,result,(unsigned long long)sequence->typeValueId,
            (unsigned long long)payload->typeValueId,transfer,payloadType,descriptor,
            any || plan->transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN ? "NULL" :
                xl6IrCgenNativeValueConverterExpr(module,plan->elementType));
    }
    return xl6OutputHasFailed(out) ? -1 : 1;
}
#include "src/cgen/xl6_ir_cgen_concurrent_queue_emit.c"
#include "src/cgen/xl6_ir_cgen_channel_emit.c"
#include "src/cgen/xl6_ir_cgen_list_target_emit.c"
static int xl6IrCgenEmitSequenceCreate(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId typeId,XL6_IrValueId result,XL6_IrNominalIdentitySetId admissionSet)
{
    char target[24]; snprintf(target,sizeof(target),"v%u",(unsigned)result);
    return xl6IrCgenEmitSequenceCreateDepth(out,module,typeId,target,admissionSet,0,NULL,0,false,true);
}
static int xl6IrCgenEmitArrayCreate(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if (!xl6IrArrayCreateRuntimeInstructionCompatible(module,instruction)) return -1;
    char target[24]; snprintf(target,sizeof(target),"v%u",(unsigned)instruction->result);
    return xl6IrCgenEmitSequenceCreateDepth(out,module,instruction->type,target,
        instruction->sequenceAdmissionIdentitySet,0,NULL,0,true,true);
}

/* Explicit data boundary renderer. IR owns the complete chain and physical
 * codecs. Cgen emits only factories, resident projection bindings and ONE
 * import call; no generated item loops, recursive conversion or memo table.
 * Source producer/cast authorization will select this separately from the
 * native target recipe. Validate everything before emitting any C. */
static int xl6IrCgenEmitArrayImportChain(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId typeId,XL6_IrValueId result,const char* source,bool sourceOwned)
{
    size_t count=0; XL6_IrTypeId types[65]; XL6_IrNominalIdentitySetId set=0;
    if (!source || !xl6IrArrayImportChainRecipe(module,typeId,types,65,&count) ||
        !xl6IrSequenceConstructionRecipe(module,typeId,XL6_IR_SEQUENCE_ROLE_ARRAY,&set)) return -1;
    bool mixed=false;
    for (size_t i=0;i<count;++i)
        if (xl6IrTypeGet(&module->types,types[i])->collectionAbi!=XL6_IR_COLLECTION_ARRAY) mixed=true;
    char target[24]; snprintf(target,sizeof(target),"v%u",(unsigned)result);
    xl6OutputPrintf(out,"    { xvalue* xlir_array_import_seeds[%u] = {0};\n",(unsigned)count);
    if (xl6IrCgenEmitSequenceCreateDepth(out,module,typeId,target,set,0,"xlir_array_import_seeds",count,false,true)<0) return -1;
    if (mixed) {
        xl6OutputPrintf(out,"    const XL6_CollectionImportProviderV1* xlir_array_import_providers[%u] = {",(unsigned)count);
        if (!xl6IrCgenEmitImportProviders(out,module,types,count)) return -1;
        xl6OutputPrintf(out,"};\n    const XL6_CollectionImportPlanV1 xlir_array_import_plan = {sizeof(XL6_CollectionImportPlanV1),"
            "xlir_array_import_providers,XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1,0,0,false};\n"
            "    xrtValueRelease(%s);\n    %s = %s(xlir_array_import_seeds,%u,%s,&xlir_array_import_plan); }\n",
            target,target,sourceOwned ? "xl6CollectionImportChainSourceTakeV1" : "xl6CollectionImportChainTakeV1",
            (unsigned)count,source);
        return xl6OutputHasFailed(out) ? -1 : 1;
    }
    xl6OutputPrintf(out,"    xrtValueRelease(%s);\n"
        "    %s = %s(xlir_array_import_seeds, %u, %s); }\n",
        target,target,sourceOwned ? "xl6ArrayImportChainSourceTakeV1" : "xl6ArrayImportChainTakeV1",
        (unsigned)count,source);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenEmitValueCollectionImportChain(XL6_Output* out,const XL6_IrModule* module,
    XL6_IrTypeId typeId,XL6_IrValueId result,const char* source)
{
    XL6_IrTypeId types[65]; XL6_IrValueCollectionRecipe recipe={0};
    size_t count=0;
    if (!source || !xl6IrValueCollectionConstructionRecipe(module,typeId,&recipe) ||
        !xl6IrCollectionImportChainRecipe(module,typeId,types,65,&count)) return -1;
    char target[24]; snprintf(target,sizeof(target),"v%u",(unsigned)result);
    xl6OutputPrintf(out,"    { xvalue* xlir_list_import_seeds[%u] = {0};\n",(unsigned)count);
    if (xl6IrCgenEmitValueCollectionTargetCreateDepth(out,module,typeId,recipe.family,target,
        recipe.admissionIdentitySet,0,"xlir_list_import_seeds",count)<0) return -1;
    xl6OutputPrintf(out,"    xrtValueRelease(%s);\n"
        "    const XL6_CollectionImportProviderV1* xlir_list_import_providers[%u] = {",target,(unsigned)count);
    if (!xl6IrCgenEmitImportProviders(out,module,types,count)) return -1;
    xl6OutputPrintf(out,"};\n"
        "    const XL6_CollectionImportPlanV1 xlir_list_import_plan = {sizeof(XL6_CollectionImportPlanV1),"
        "xlir_list_import_providers,XL6_COLLECTION_IMPORT_LANGUAGE_INPUT_V1,0,0,false};\n"
        "    %s = xl6CollectionImportChainSourceTakeV1(xlir_list_import_seeds,%u,%s,&xlir_list_import_plan); }\n",
        target,(unsigned)count,source);
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenEmitStackRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* receiver;
    const XL6_IrType* stack;
    const XL6_IrType* payload;
    const char* payloadType;
    const char* payloadAbi;
    const char* runtimeType;
    XL6_IrValueId first;
    if ( instruction == NULL ||
         !xl6IrCgenStackRuntimeOpIs(instruction->runtimeOp) ) return 0;
    if ( !xl6IrCgenRuntimeStackAbi(module, instruction) ) return -1;
    first = instruction->operandCount != 0
        ? module->operands[instruction->firstOperand] : XL6_IR_VALUE_INVALID;
    receiver = instruction->runtimeOp == XL6_IR_RUNTIME_STACK_CREATE
        ? NULL : xl6IrCgenValueDefinition(module, first, NULL);
    if ( !xl6IrCgenStackTypeAbi(
             module, receiver != NULL ? receiver->type : instruction->type,
             &stack, &payload) ) return -1;
    payloadType = xl6IrCgenType(module, instruction->payloadType);
    payloadAbi = xl6IrCgenSequenceTransferAbiToken(
        instruction->runtimeResourcePayloadAbi);
    runtimeType = xl6IrCgenNativeValueTypeExpr(module, instruction->payloadType);
    if ( payloadType == NULL || payloadAbi == NULL || runtimeType == NULL ) return -1;
#define XL6_IR_CGEN_STACK_COMMON_ARGS                                      \
    (unsigned long long)stack->typeValueId,                                \
    (unsigned long long)payload->typeValueId, payloadAbi, payloadType, runtimeType
    if (instruction->runtimeOp>=XL6_IR_RUNTIME_STACK_PUSH_BATCH &&
        instruction->runtimeOp<=XL6_IR_RUNTIME_STACK_PEEK_BATCH_AT) {
        XL6_IrValueId second=module->operands[instruction->firstOperand+1];
        if (instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PUSH_BATCH) {
            const XL6_IrInstruction* array=xl6IrCgenValueDefinition(module,second,NULL);
            xl6OutputPrintf(out,
                "    v%u = xl6StackPushBatchV1(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, v%u, 0x%016llXULL);\n",
                (unsigned)instruction->result,(unsigned)first,XL6_IR_CGEN_STACK_COMMON_ARGS,
                (unsigned)second,(unsigned long long)xl6IrTypeGet(&module->types,array->type)->typeValueId);
        } else {
            const XL6_IrType* array=xl6IrTypeGet(&module->types,instruction->type);
            const char* projection=array->sequenceLayout.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN
                ? "xl6ArrayReferenceConverterV1()" : xl6IrCgenNativeValueConverterExpr(module,instruction->payloadType);
            if (!projection) return -1;
            xl6OutputPrintf(out,
                "    v%u = xl6StackExtractArrayV1(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, 0x%016llXULL, ",
                (unsigned)instruction->result,(unsigned)first,XL6_IR_CGEN_STACK_COMMON_ARGS,
                (unsigned long long)array->typeValueId);
            if (instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PEEK_BATCH_AT)
                xl6OutputPrintf(out,"v%u, v%u",(unsigned)second,
                    (unsigned)module->operands[instruction->firstOperand+2]);
            else xl6OutputPrintf(out,"0, v%u",(unsigned)second);
            xl6OutputPrintf(out,", %s, %s);\n",
                instruction->runtimeOp==XL6_IR_RUNTIME_STACK_POP_BATCH ? "true" : "false",projection);
        }
        return xl6OutputHasFailed(out) ? -1 : 1;
    }
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_STACK_CREATE:
        if (xl6IrCgenEmitSequenceCreate(out,module,instruction->type,instruction->result,
            instruction->sequenceAdmissionIdentitySet)<0) return -1;
        break;
    case XL6_IR_RUNTIME_STACK_PUSH:
        xl6OutputPrintf(out,
            "    v%u = xl6StackPushV1(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, &v%u);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_STACK_COMMON_ARGS,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_RUNTIME_STACK_POP:
    case XL6_IR_RUNTIME_STACK_PEEK:
    case XL6_IR_RUNTIME_STACK_PEEK_AT:
        /* Like a physical return assignment, each evaluation replaces the
         * consumed SSA spelling. It must not feed a stale prior-iteration
         * pointer into the initialized-output native ABI. */
        xl6OutputPrintf(out, "    v%u = (%s){0};\n",
            (unsigned int)instruction->result, payloadType);
        xl6OutputPrintf(out,
            "    bool projection_ok_%u = xl6StackTakeV1(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, ",
            (unsigned int)instruction->result, (unsigned int)first, XL6_IR_CGEN_STACK_COMMON_ARGS);
        if ( instruction->runtimeOp == XL6_IR_RUNTIME_STACK_PEEK_AT )
            xl6OutputPrintf(out, "v%u", (unsigned int)module->operands[instruction->firstOperand + 1]);
        else xl6OutputPuts("0", out);
        xl6OutputPrintf(out, ", &v%u, %s);\n", (unsigned int)instruction->result,
            instruction->runtimeOp == XL6_IR_RUNTIME_STACK_POP ? "true" : "false");
        break;
    case XL6_IR_RUNTIME_STACK_RESERVE:
        xl6OutputPrintf(out,
            "    v%u = xl6StackReserveV1(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, v%u);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_STACK_COMMON_ARGS,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_RUNTIME_STACK_TRY_POP:
    case XL6_IR_RUNTIME_STACK_TRY_PEEK:
    case XL6_IR_RUNTIME_STACK_TRY_PEEK_AT:
        {
            const XL6_IrType* product = xl6IrTypeGet(&module->types, instruction->type);
            int tagged = xl6IrOptionalResultPayloadTagged(module,product->params[1],instruction->payloadType);
            xl6OutputPrintf(out, "    v%u = (%s){0};\n",
                (unsigned int)instruction->result, xl6IrCgenType(module, instruction->type));
            xl6OutputPrintf(out,
                "    bool projection_ok_%u = xl6StackTryTakeV2(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, ",
                (unsigned int)instruction->result, (unsigned int)first, XL6_IR_CGEN_STACK_COMMON_ARGS);
            if ( instruction->runtimeOp == XL6_IR_RUNTIME_STACK_TRY_PEEK_AT )
                xl6OutputPrintf(out, "v%u", (unsigned int)module->operands[instruction->firstOperand + 1]);
            else xl6OutputPuts("0", out);
            xl6OutputPrintf(out, ", &v%u.e1%s, %s, &v%u.e0);\n", (unsigned int)instruction->result,
                tagged ? ".value" : "", instruction->runtimeOp == XL6_IR_RUNTIME_STACK_TRY_POP ? "true" : "false",
                (unsigned int)instruction->result);
            if ( tagged ) xl6OutputPrintf(out, "    v%u.e1.hasValue = v%u.e0;\n",
                (unsigned int)instruction->result, (unsigned int)instruction->result);
        }
        break;
    case XL6_IR_RUNTIME_STACK_CLONE:
        xl6OutputPrintf(out,
            "    v%u = xl6StackCloneV1(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_STACK_COMMON_ARGS);
        break;
    case XL6_IR_RUNTIME_STACK_EQUALS:
        xl6OutputPrintf(out,
            "    v%u = xl6StackEqualsV1(v%u, v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned int)module->operands[instruction->firstOperand + 1],
            XL6_IR_CGEN_STACK_COMMON_ARGS);
        break;
    case XL6_IR_RUNTIME_STACK_TRIM:
        xl6OutputPrintf(out,
            "    v%u = xl6StackTrimV1(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_STACK_COMMON_ARGS);
        break;
    case XL6_IR_RUNTIME_STACK_CLEAR:
        xl6OutputPrintf(out,
            "    v%u = xl6StackClearV1(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s);\n",
            (unsigned int)instruction->result, (unsigned int)first, XL6_IR_CGEN_STACK_COMMON_ARGS);
        break;
    case XL6_IR_RUNTIME_STACK_COUNT:
    case XL6_IR_RUNTIME_STACK_CAPACITY:
        xl6OutputPrintf(out,
            "    v%u = xl6StackSizeV1(v%u, 0x%016llXULL, 0x%016llXULL, %s, sizeof(%s), %s, %s);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            XL6_IR_CGEN_STACK_COMMON_ARGS,
            instruction->runtimeOp == XL6_IR_RUNTIME_STACK_CAPACITY
                ? "true" : "false");
        break;
    default:
#undef XL6_IR_CGEN_STACK_COMMON_ARGS
        return 0;
    }
#undef XL6_IR_CGEN_STACK_COMMON_ARGS
    return xl6OutputHasFailed(out) ? -1 : 1;
}

#include "src/cgen/xl6_ir_cgen_tree_emit.c"
