/* Included by xl6_ir.c. Stack batching has one exact logical/native schema;
 * no boxed Array conversion or source-name inference survives specialization. */
int xl6IrStackBatchResourceOpIs(XL6_IrRuntimeResourceOp op)
{
    return op>=XL6_IR_RUNTIME_RESOURCE_OP_STACK_PUSH_BATCH &&
        op<=XL6_IR_RUNTIME_RESOURCE_OP_STACK_PEEK_BATCH_AT;
}
static XL6_IrRuntimeResourceOp xl6IrStackBatchResourceOp(XL6_IrRuntimeOp op)
{
    switch (op) {
    case XL6_IR_RUNTIME_STACK_PUSH_BATCH: return XL6_IR_RUNTIME_RESOURCE_OP_STACK_PUSH_BATCH;
    case XL6_IR_RUNTIME_STACK_POP_BATCH: return XL6_IR_RUNTIME_RESOURCE_OP_STACK_POP_BATCH;
    case XL6_IR_RUNTIME_STACK_PEEK_BATCH: return XL6_IR_RUNTIME_RESOURCE_OP_STACK_PEEK_BATCH;
    case XL6_IR_RUNTIME_STACK_PEEK_BATCH_AT: return XL6_IR_RUNTIME_RESOURCE_OP_STACK_PEEK_BATCH_AT;
    default: return XL6_IR_RUNTIME_RESOURCE_OP_INVALID;
    }
}
static int xl6IrStackBatchInteger(const XL6_IrModule* module,XL6_IrTypeId id)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    return type && type->kind==XL6_IR_TYPE_BUILTIN && type->cAbi==XL6_IR_C_ABI_I64 &&
        type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64 &&
        type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL && type->nullAbi==XL6_IR_NULL_ABI_NONE;
}
static int xl6IrStackBatchLogicalSignature(const XL6_IrModule* module,XL6_IrRuntimeResourceOp op,
    XL6_IrTypeId receiver,XL6_IrTypeId result,const XL6_IrTypeId* operands,size_t count)
{
    if (!module || !xl6IrStackBatchResourceOpIs(op) || !operands ||
        count!=(op==XL6_IR_RUNTIME_RESOURCE_OP_STACK_PEEK_BATCH_AT ? 3u : 2u) ||
        operands[0]!=receiver) return 0;
    const XL6_IrType* stack=xl6IrTypeGet(&module->types,receiver);
    int push=op==XL6_IR_RUNTIME_RESOURCE_OP_STACK_PUSH_BATCH;
    const XL6_IrType* array=xl6IrTypeGet(&module->types,push ? operands[1] : result);
    if (!stack || stack->kind!=XL6_IR_TYPE_GENERIC_INSTANCE ||
        stack->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_STACK ||
        stack->paramCount!=1 || !stack->params || !array ||
        array->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || array->collectionAbi!=XL6_IR_COLLECTION_ARRAY ||
        array->paramCount!=1 || !array->params || array->params[0]!=stack->params[0] ||
        array->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
        array->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED) return 0;
    if (push) {
        const XL6_IrType* boolean=xl6IrTypeGet(&module->types,result);
        return boolean && boolean->kind==XL6_IR_TYPE_BUILTIN && boolean->cAbi==XL6_IR_C_ABI_BOOL &&
            boolean->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL && boolean->nullAbi==XL6_IR_NULL_ABI_NONE;
    }
    return xl6IrStackBatchInteger(module,operands[1]) &&
        (count!=3 || xl6IrStackBatchInteger(module,operands[2]));
}
int xl6IrStackBatchSignatureCompatible(const XL6_IrModule* module,XL6_IrRuntimeResourceOp op,
    XL6_IrTypeId receiver,XL6_IrTypeId result,const XL6_IrTypeId* operands,size_t count)
{
    if (!xl6IrStackBatchLogicalSignature(module,op,receiver,result,operands,count) ||
        !xl6IrStackConstructionContract(module,receiver,NULL)) return 0;
    XL6_IrTypeId arrayId=op==XL6_IR_RUNTIME_RESOURCE_OP_STACK_PUSH_BATCH ? operands[1] : result;
    if (!xl6IrSequenceConstructionRecipe(module,arrayId,XL6_IR_SEQUENCE_ROLE_ARRAY,NULL)) return 0;
    const XL6_IrSequenceLayout* stack=&xl6IrTypeGet(&module->types,receiver)->sequenceLayout;
    const XL6_IrSequenceLayout* array=&xl6IrTypeGet(&module->types,arrayId)->sequenceLayout;
    return stack->elementType==array->elementType && stack->slotAbi==array->slotAbi &&
        stack->transferAbi==array->transferAbi && stack->codecAbi==array->codecAbi;
}
int xl6IrStackBatchInstructionCompatible(const XL6_IrModule* module,const XL6_IrInstruction* instruction)
{
    if (!module || !instruction || instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
        !xl6IrStackBatchResourceOpIs(xl6IrStackBatchResourceOp(instruction->runtimeOp)) ||
        instruction->operandCount>3 || instruction->firstOperand>module->operandCount ||
        instruction->operandCount>module->operandCount-instruction->firstOperand ||
        !instruction->result || instruction->immediateText || instruction->immediateData ||
        instruction->immediateSize || instruction->opcode || instruction->targetSymbol ||
        instruction->targetValue || instruction->calleeType || instruction->runtimeIdentityType ||
        instruction->runtimeResourceKeyType || instruction->runtimeResourceKeyPayloadAbi ||
        instruction->runtimeResourceKeyOrderAbi || instruction->sequenceAdmissionIdentitySet) return 0;
    XL6_IrTypeId operands[3]={0};
    for (size_t i=0;i<instruction->operandCount;++i) {
        const XL6_IrInstruction* definition=xl6IrValueDefinition(module,
            module->operands[instruction->firstOperand+i],NULL);
        if (!definition) return 0;
        operands[i]=definition->type;
    }
    if (!xl6IrStackBatchSignatureCompatible(module,xl6IrStackBatchResourceOp(instruction->runtimeOp),
        operands[0],instruction->type,operands,instruction->operandCount)) return 0;
    const XL6_IrSequenceLayout* stack=&xl6IrTypeGet(&module->types,operands[0])->sequenceLayout;
    return instruction->payloadType==stack->elementType &&
        instruction->runtimeResourcePayloadAbi==stack->transferAbi;
}
