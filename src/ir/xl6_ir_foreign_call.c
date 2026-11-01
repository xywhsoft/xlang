/* Declaration-driven foreign calls. A physical C operation is separate from
 * the language admission transaction. The verifier re-proves both independently
 * and never trusts that a producer was emitted by the current lowering pass. */
static int xl6IrForeignOperands(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction, const XL6_IrValueId* operands, size_t count)
{
    return instruction && instruction->operandCount == count &&
        instruction->firstOperand <= module->operandCount &&
        count <= module->operandCount - instruction->firstOperand &&
        (!count || !memcmp(module->operands + instruction->firstOperand, operands, count * sizeof(*operands)));
}

static const XL6_IrBlock* xl6IrForeignBlock(const XL6_IrModule* module,
    const XL6_IrFunction* function, XL6_IrBlockId id, size_t minimum)
{
    if (!id || id <= function->firstBlock || id > function->firstBlock + function->blockCount ||
        id > module->blockCount) return NULL;
    const XL6_IrBlock* block = &module->blocks[id - 1];
    return block->firstInstruction <= module->instructionCount && block->instructionCount >= minimum &&
        block->instructionCount <= module->instructionCount - block->firstInstruction ? block : NULL;
}

static int xl6IrForeignOnlyEntries(const XL6_IrModule* module, XL6_IrBlockId block,
    size_t firstEdge, size_t secondEdge)
{
    unsigned entries = 0;
    if (block == module->entryBlock) return 0;
    for (size_t i = 0; i < module->functionCount; ++i)
        if (block == module->functions[i].entryBlock) return 0;
    for (size_t i = 0; i < module->blockCount; ++i) {
        const XL6_IrBlock* source = &module->blocks[i];
        if (!source->instructionCount || source->firstInstruction > module->instructionCount ||
            source->instructionCount > module->instructionCount - source->firstInstruction) return 0;
        size_t index = source->firstInstruction + source->instructionCount - 1;
        const XL6_IrInstruction* edge = &module->instructions[index];
        if (edge->trueTarget != block && edge->falseTarget != block) continue;
        if (index != firstEdge && index != secondEdge) return 0;
        ++entries;
    }
    return entries == (secondEdge == SIZE_MAX ? 1u : 2u);
}

static int xl6IrVerifyNativeStatusOut(const XL6_IrModule* module,
    const XL6_IrFunction* function, const XL6_IrBlock* block, size_t index)
{
    const XL6_IrInstruction* call = &module->instructions[index];
    const XL6_IrCallTarget* target = xl6IrCallTargetGet(module, call->targetSymbol);
    const XL6_IrNode* declaration = target ? xl6IrNodeGet(module, target->declaration) : NULL;
    const XL6_IrType* pair = xl6IrTypeGet(&module->types, call->type);
    const XL6_IrType* rawType = target ? xl6IrTypeGet(&module->types, target->returnType) : NULL;
    if (!block || !declaration || !declaration->foreignCall.outputIndexPlusOne ||
        declaration->foreignCall.outputIndexPlusOne > target->parameterCount + 1 ||
        !(declaration->flags & XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
        (declaration->flags & (XL6_AST_FLAG_FOREIGN_PURE | XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
        target->kind != XL6_IR_CALL_TARGET_EXTERN_C || target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_NONE ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE || target->nativeServicesRequired || target->nativeContextAbi ||
        target->nativeResultAbi || target->nativeArrayResultAbi || target->errorAbi != XL6_IR_ERROR_ABI_NO_RAISE ||
        !rawType || !rawType->foreignHeader || rawType->kind != XL6_IR_TYPE_CLASS ||
        rawType->cAbi != XL6_IR_C_ABI_NATIVE_HANDLE || rawType->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
        !pair || pair->kind != XL6_IR_TYPE_TUPLE || pair->paramCount != 2 || !pair->params ||
        pair->params[1] != target->returnType || pair->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
        !call->result || call->operandCount != target->parameterCount || call->targetValue || call->calleeType ||
        call->runtimeOp || call->opcode || call->immediateText || call->payloadType || call->sourcePayloadType ||
        call->elementIndex != SIZE_MAX || index + 7 != block->firstInstruction + block->instructionCount) return 0;
    const XL6_IrType* statusType = xl6IrTypeGet(&module->types, pair->params[0]);
    if (!statusType || statusType->cAbi != XL6_IR_C_ABI_I64 ||
        statusType->integerTarget != XL6_IR_INTEGER_TARGET_SIGNED_64) return 0;
    for (size_t p = 0; p < target->parameterCount; ++p) {
        const XL6_IrInstruction* argument = xl6IrValueDefinition(module, module->operands[call->firstOperand + p], NULL);
        const XL6_IrType* type = xl6IrTypeGet(&module->types, target->parameterTypes[p]);
        if (!argument || argument->type != target->parameterTypes[p] || !type ||
            type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL) return 0;
    }
    const XL6_IrCallTarget* rollback = xl6IrCallTargetGet(module, declaration->foreignCall.rollbackSymbol);
    if (!rollback || rollback->kind != XL6_IR_CALL_TARGET_EXTERN_C || rollback->returnType ||
        rollback->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_NONE || rollback->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
        rollback->parameterCount != 1 || rollback->parameterTypes[0] != target->returnType ||
        rollback->parameterPhysicalAbis[0] != XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
        rollback->errorAbi != XL6_IR_ERROR_ABI_NO_RAISE) return 0;
    const XL6_IrInstruction *status = call + 1, *raw = call + 2, *code = call + 3,
        *equal = call + 4, *null = call + 5, *branch = call + 6;
    char expected[40]; snprintf(expected, sizeof(expected), "%dLL", declaration->foreignCall.successCode);
    XL6_IrValueId compare[2] = {status->result, code->result};
    if (status->kind != XL6_IR_INSTR_PRODUCT_GET || status->elementIndex || status->type != pair->params[0] ||
        raw->kind != XL6_IR_INSTR_PRODUCT_GET || raw->elementIndex != 1 || raw->type != target->returnType ||
        !xl6IrForeignOperands(module, status, &call->result, 1) || !xl6IrForeignOperands(module, raw, &call->result, 1) ||
        code->kind != XL6_IR_INSTR_CONST || code->type != pair->params[0] || !code->immediateText ||
        strcmp(code->immediateText, expected) || equal->kind != XL6_IR_INSTR_BINARY || equal->opcode != XL6_IR_OPCODE_EQ ||
        !xl6IrForeignOperands(module, equal, compare, 2) || null->kind != XL6_IR_INSTR_NULL_TEST ||
        !xl6IrForeignOperands(module, null, &raw->result, 1) || branch->kind != XL6_IR_INSTR_BRANCH ||
        !xl6IrForeignOperands(module, branch, &equal->result, 1)) return 0;
    const XL6_IrBlock* statusBlock = xl6IrForeignBlock(module, function, branch->trueTarget, 1);
    if (!statusBlock || statusBlock->instructionCount != 1) return 0;
    const XL6_IrInstruction* present = &module->instructions[statusBlock->firstInstruction];
    if (present->kind != XL6_IR_INSTR_BRANCH || !xl6IrForeignOperands(module, present, &null->result, 1) ||
        present->trueTarget != branch->falseTarget) return 0;
    const XL6_IrBlock* success = xl6IrForeignBlock(module, function, present->falseTarget, 1);
    const XL6_IrBlock* failure = xl6IrForeignBlock(module, function, branch->falseTarget, 4);
    if (!success || !failure || failure->instructionCount != 4 ||
        !xl6IrForeignOnlyEntries(module, branch->trueTarget, index + 6, SIZE_MAX) ||
        !xl6IrForeignOnlyEntries(module, present->falseTarget, statusBlock->firstInstruction, SIZE_MAX) ||
        !xl6IrForeignOnlyEntries(module, branch->falseTarget, index + 6, statusBlock->firstInstruction)) return 0;
    const XL6_IrInstruction* store = &module->instructions[failure->firstInstruction];
    const XL6_IrInstruction *load = store + 1, *empty = store + 2, *dispatch = store + 3;
    if (store->kind != XL6_IR_INSTR_STORE || store->type != target->returnType ||
        !xl6IrForeignOperands(module, store, &raw->result, 1) || load->kind != XL6_IR_INSTR_LOAD ||
        load->targetSymbol != store->targetSymbol || load->type != target->returnType ||
        empty->kind != XL6_IR_INSTR_NULL_TEST || !xl6IrForeignOperands(module, empty, &load->result, 1) ||
        dispatch->kind != XL6_IR_INSTR_BRANCH || !xl6IrForeignOperands(module, dispatch, &empty->result, 1)) return 0;
    const XL6_IrBlock* destroy = xl6IrForeignBlock(module, function, dispatch->falseTarget, 2);
    const XL6_IrBlock* raise = xl6IrForeignBlock(module, function, dispatch->trueTarget, 2);
    if (!destroy || destroy->instructionCount != 2 || !raise ||
        !xl6IrForeignOnlyEntries(module, dispatch->falseTarget, failure->firstInstruction + 3, SIZE_MAX)) return 0;
    const XL6_IrInstruction *dispose = &module->instructions[destroy->firstInstruction], *join = dispose + 1;
    if (dispose->kind != XL6_IR_INSTR_CALL || dispose->targetSymbol != rollback->symbol ||
        !xl6IrForeignOperands(module, dispose, &load->result, 1) || join->kind != XL6_IR_INSTR_JUMP ||
        join->trueTarget != dispatch->trueTarget ||
        !xl6IrForeignOnlyEntries(module, dispatch->trueTarget, failure->firstInstruction + 3, destroy->firstInstruction + 1)) return 0;
    /* Diagnostic materialization may itself fail, but only after rollback.
     * Its success block must start the declared checked error, not publish raw. */
    const XL6_IrInstruction *message = &module->instructions[raise->firstInstruction], *error = message + 1;
    const XL6_IrBlock* raised = error->kind == XL6_IR_INSTR_ERROR_TEST
        ? xl6IrForeignBlock(module, function, error->trueTarget, 1) : NULL;
    const XL6_IrInstruction* raisedError = raised ? &module->instructions[raised->firstInstruction] : NULL;
    if (raise->instructionCount != 2 || message->kind != XL6_IR_INSTR_RUNTIME_CALL ||
        message->runtimeOp != XL6_IR_RUNTIME_STRING_DUP || !raisedError ||
        raisedError->kind != XL6_IR_INSTR_ERROR_RAISE || !raisedError->immediateText ||
        strcmp(raisedError->immediateText, "\"NativeError\"") ||
        !xl6IrForeignOperands(module, raisedError, &message->result, 1)) return 0;
    for (size_t i = function->firstInstruction; i < function->firstInstruction + function->instructionCount; ++i) {
        const XL6_IrInstruction* use = &module->instructions[i];
        if (use->firstOperand > module->operandCount || use->operandCount > module->operandCount - use->firstOperand) return 0;
        if (use->targetValue == call->result || use->targetValue == raw->result) return 0;
        if (use->targetValue == status->result || use->targetValue == equal->result ||
            use->targetValue == null->result || use->targetValue == load->result) return 0;
        if (use->targetSymbol == store->targetSymbol && use != store && use != load) return 0;
        for (size_t p = 0; p < use->operandCount; ++p) {
            XL6_IrValueId value = module->operands[use->firstOperand + p];
            if (value == call->result && use != status && use != raw) return 0;
            if (value == status->result && use != equal) return 0;
            if (value == raw->result && use != null && use != store &&
                i != success->firstInstruction &&
                !xl6IrOwnedInputDominates(module, function, success->firstInstruction, i)) return 0;
        }
    }
    return 1;
}
