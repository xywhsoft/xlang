/* Validate closed char, string-scalar and bytes runtime instruction ABIs. */
static int xl6IrCgenPrimitiveSequenceRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation == XL6_IR_RUNTIME_CHAR_TO_STRING ||
           operation == XL6_IR_RUNTIME_BYTES_CONCAT ||
           operation == XL6_IR_RUNTIME_BYTES_EQUAL ||
           operation == XL6_IR_RUNTIME_BYTES_ORDER_LT ||
           operation == XL6_IR_RUNTIME_BYTES_ORDER_LE ||
           operation == XL6_IR_RUNTIME_BYTES_ORDER_GT ||
           operation == XL6_IR_RUNTIME_BYTES_ORDER_GE ||
           operation == XL6_IR_RUNTIME_BYTES_LENGTH ||
           operation == XL6_IR_RUNTIME_BYTES_AT ||
           operation == XL6_IR_RUNTIME_BYTES_SET_COPY ||
           operation == XL6_IR_RUNTIME_STRING_SCALAR_AT ||
           operation == XL6_IR_RUNTIME_STRING_SCALAR_SET_COPY ||
           operation == XL6_IR_RUNTIME_STRING_CONCAT_CHAR_RIGHT ||
           operation == XL6_IR_RUNTIME_STRING_CONCAT_CHAR_LEFT;
}

static int xl6IrCgenPrimitiveSequenceRuntimeAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* operands[3] = { NULL, NULL, NULL };
    const XL6_IrType* types[3] = { NULL, NULL, NULL };
    const XL6_IrType* result;
    size_t i;
    if ( module == NULL || instruction == NULL ||
         !xl6IrCgenPrimitiveSequenceRuntimeOpIs(instruction->runtimeOp) ||
         instruction->result == XL6_IR_VALUE_INVALID ||
         instruction->immediateText != NULL ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount > 3 ||
         instruction->operandCount > module->operandCount - instruction->firstOperand ||
         (result = xl6IrTypeGet(&module->types, instruction->type)) == NULL )
        return 0;
    for ( i = 0; i < instruction->operandCount; ++i ) {
        operands[i] = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + i], NULL);
        types[i] = operands[i] != NULL
            ? xl6IrTypeGet(&module->types, operands[i]->type) : NULL;
        if ( types[i] == NULL ) return 0;
    }
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_CHAR_TO_STRING:
        return instruction->operandCount == 1 &&
            types[0]->cAbi == XL6_IR_C_ABI_CHAR_U32 &&
            result->cAbi == XL6_IR_C_ABI_MANAGED_STRING;
    case XL6_IR_RUNTIME_BYTES_CONCAT:
        return instruction->operandCount == 2 &&
            types[0]->cAbi == XL6_IR_C_ABI_MANAGED_BYTES &&
            operands[0]->type == operands[1]->type &&
            instruction->type == operands[0]->type;
    case XL6_IR_RUNTIME_BYTES_EQUAL:
    case XL6_IR_RUNTIME_BYTES_ORDER_LT:
    case XL6_IR_RUNTIME_BYTES_ORDER_LE:
    case XL6_IR_RUNTIME_BYTES_ORDER_GT:
    case XL6_IR_RUNTIME_BYTES_ORDER_GE:
        return instruction->operandCount == 2 &&
            types[0]->cAbi == XL6_IR_C_ABI_MANAGED_BYTES &&
            operands[0]->type == operands[1]->type &&
            result->cAbi == XL6_IR_C_ABI_BOOL;
    case XL6_IR_RUNTIME_BYTES_LENGTH:
        return instruction->operandCount == 1 &&
            types[0]->cAbi == XL6_IR_C_ABI_MANAGED_BYTES &&
            result->cAbi == XL6_IR_C_ABI_I64;
    case XL6_IR_RUNTIME_BYTES_AT:
        return instruction->operandCount == 2 &&
            types[0]->cAbi == XL6_IR_C_ABI_MANAGED_BYTES &&
            types[1]->cAbi == XL6_IR_C_ABI_I64 &&
            result->cAbi == XL6_IR_C_ABI_I64 &&
            result->integerTarget == XL6_IR_INTEGER_TARGET_UNSIGNED_8;
    case XL6_IR_RUNTIME_BYTES_SET_COPY:
        return instruction->operandCount == 3 &&
            types[0]->cAbi == XL6_IR_C_ABI_MANAGED_BYTES &&
            types[1]->cAbi == XL6_IR_C_ABI_I64 &&
            types[2]->cAbi == XL6_IR_C_ABI_I64 &&
            types[2]->integerTarget == XL6_IR_INTEGER_TARGET_UNSIGNED_8 &&
            instruction->type == operands[0]->type;
    case XL6_IR_RUNTIME_STRING_SCALAR_AT:
        return instruction->operandCount == 2 &&
            types[0]->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
            types[1]->cAbi == XL6_IR_C_ABI_I64 &&
            result->cAbi == XL6_IR_C_ABI_CHAR_U32;
    case XL6_IR_RUNTIME_STRING_SCALAR_SET_COPY:
        return instruction->operandCount == 3 &&
            types[0]->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
            types[1]->cAbi == XL6_IR_C_ABI_I64 &&
            types[2]->cAbi == XL6_IR_C_ABI_CHAR_U32 &&
            instruction->type == operands[0]->type;
    case XL6_IR_RUNTIME_STRING_CONCAT_CHAR_RIGHT:
        return instruction->operandCount == 2 &&
            types[0]->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
            types[1]->cAbi == XL6_IR_C_ABI_CHAR_U32 &&
            instruction->type == operands[0]->type;
    case XL6_IR_RUNTIME_STRING_CONCAT_CHAR_LEFT:
        return instruction->operandCount == 2 &&
            types[0]->cAbi == XL6_IR_C_ABI_CHAR_U32 &&
            types[1]->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
            instruction->type == operands[1]->type;
    default:
        return 0;
    }
}
