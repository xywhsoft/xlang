/* Included by xl6_ir.c. The public queue has one closed logical/physical
 * contract shared by Typed XLIR, lowering, verification and C emission.
 * In particular a closure's invocation ABI is NOT its resident slot ABI. */

static int xl6IrConcurrentQueueIntegerType(const XL6_IrModule* module,XL6_IrTypeId id)
{
    const XL6_IrType* type=module ? xl6IrTypeGet(&module->types,id) : NULL;
    return type && type->kind==XL6_IR_TYPE_BUILTIN && type->cAbi==XL6_IR_C_ABI_I64 &&
        type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64 &&
        type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        type->nullAbi==XL6_IR_NULL_ABI_NONE && !type->paramCount && !type->returnCount;
}

static int xl6IrConcurrentQueueBoolType(const XL6_IrModule* module,XL6_IrTypeId id)
{
    const XL6_IrType* type=module ? xl6IrTypeGet(&module->types,id) : NULL;
    return type && type->kind==XL6_IR_TYPE_BUILTIN && type->cAbi==XL6_IR_C_ABI_BOOL &&
        type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        type->nullAbi==XL6_IR_NULL_ABI_NONE && !type->paramCount && !type->returnCount;
}

static int xl6IrConcurrentQueueBatchResult(const XL6_IrModule* module,
    XL6_IrTypeId id,XL6_IrTypeId arrayType,int push)
{
    const XL6_IrType* type=module ? xl6IrTypeGet(&module->types,id) : NULL;
    if (!type || type->kind!=XL6_IR_TYPE_TUPLE || type->cAbi!=XL6_IR_C_ABI_PRODUCT ||
        type->paramCount!=2 || !type->params || type->returnCount ||
        type->nullAbi!=XL6_IR_NULL_ABI_NONE ||
        !xl6IrConcurrentQueueIntegerType(module,type->params[0])) return 0;
    return push ? xl6IrConcurrentQueueIntegerType(module,type->params[1]) : type->params[1]==arrayType;
}

XL6_IrRuntimeResourcePayloadAbi xl6IrRuntimeResourcePayloadAbiForOwnerType(
    const XL6_IrModule* module,XL6_IrTypeId owner,XL6_IrTypeId payload)
{
    const XL6_IrType* queue=module ? xl6IrTypeGet(&module->types,owner) : NULL;
    if(queue && queue->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_SLOT_MAP) {
        XL6_IrValueCollectionRecipe recipe={0};
        return xl6IrSlotMapConstructionRecipe(module,owner,&recipe) && recipe.elementType==payload
            ? recipe.transferAbi : XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID;
    }
    if (queue && xl6IrRuntimeResourceAbiIsConcurrentQueue(queue->runtimeResourceAbi)) {
        XL6_IrTypeId schemaId=XL6_IR_TYPE_INVALID;
        if (!xl6IrConcurrentQueueConstructionRecipe(module,owner,NULL,&schemaId,NULL))
            return XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID;
        const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
        return schema && schema->sequenceLayout.elementType==payload ?
            schema->sequenceLayout.transferAbi : XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID;
    }
    return xl6IrRuntimeResourcePayloadAbiForType(module,payload);
}

XL6_IrErrorAbi xl6IrRuntimeResourceErrorAbiForOwnerType(const XL6_IrModule* module,
    XL6_IrRuntimeResourceOp operation,XL6_IrTypeId owner)
{
    const XL6_IrType* type=module ? xl6IrTypeGet(&module->types,owner) : NULL;
    if (xl6IrRuntimeResourceOpIsSpecialQueue(operation) && type &&
        xl6IrRuntimeResourceAbiIsConcurrentQueue(type->runtimeResourceAbi)) return XL6_IR_ERROR_ABI_MAY_RAISE;
    return xl6IrRuntimeResourceOpMayRaise(operation) ? XL6_IR_ERROR_ABI_MAY_RAISE : XL6_IR_ERROR_ABI_NO_RAISE;
}

static int xl6IrConcurrentQueueLogicalSignature(const XL6_IrModule* module,
    XL6_IrRuntimeResourceOp operation,XL6_IrTypeId receiver,XL6_IrTypeId result,
    const XL6_IrTypeId* operands,size_t count,XL6_IrTypeId schemaId,int dependent)
{
    int creates=operation==XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CREATE;
    if (count>2 || (count && !operands)) return 0;
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    if (!schema) return 0;
    XL6_IrTypeId element=schema->sequenceLayout.elementType;
    if (creates) return !receiver && count==1 && xl6IrConcurrentQueueIntegerType(module,operands[0]);
    if (!count || operands[0]!=receiver) return 0;
    switch (operation) {
    case XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_TRY_PUSH:
        return count==2 && operands[1]==element && xl6IrConcurrentQueueIntegerType(module,result);
    case XL6_IR_RUNTIME_RESOURCE_OP_CONCURRENT_QUEUE_TRY_POP: {
        const XL6_IrType* tuple=xl6IrTypeGet(&module->types,result);
        if (dependent) {
            if (count!=1 || !tuple || tuple->kind!=XL6_IR_TYPE_TUPLE ||
                tuple->paramCount!=2 || !tuple->params ||
                !xl6IrConcurrentQueueIntegerType(module,tuple->params[0])) return 0;
            const XL6_IrType* value=xl6IrTypeGet(&module->types,tuple->params[1]);
            const XL6_IrType* payload=xl6IrTypeGet(&module->types,element);
            return value && payload && ((tuple->params[1]==element && payload->kind==XL6_IR_TYPE_NULLABLE) ||
                (value->kind==XL6_IR_TYPE_NULLABLE && value->paramCount==1 && value->params && value->params[0]==element));
        }
        return count==1 && xl6IrOptionalTryResultCompatible(module,result,element,XL6_IR_C_ABI_I64) &&
            tuple && xl6IrConcurrentQueueIntegerType(module,tuple->params[0]);
    }
    case XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_PUSH_BATCH:
        return count==2 && operands[1]==schemaId && xl6IrConcurrentQueueBatchResult(module,result,schemaId,1);
    case XL6_IR_RUNTIME_RESOURCE_OP_CONCURRENT_QUEUE_POP_BATCH:
        return count==2 && xl6IrConcurrentQueueIntegerType(module,operands[1]) &&
            xl6IrConcurrentQueueBatchResult(module,result,schemaId,0);
    case XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CLOSE:
        return count==1 && result==XL6_IR_TYPE_INVALID;
    case XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_COUNT:
    case XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CAPACITY:
        return count==1 && xl6IrConcurrentQueueIntegerType(module,result);
    case XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_RESET:
    case XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_IS_CLOSED:
    case XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_IS_DRAINED:
        return count==1 && xl6IrConcurrentQueueBoolType(module,result);
    default: return 0; /* No storage references, blocking operations or legacy Into. */
    }
}

int xl6IrConcurrentQueueSignatureCompatible(const XL6_IrModule* module,
    XL6_IrRuntimeResourceOp operation,XL6_IrTypeId receiver,XL6_IrTypeId result,
    const XL6_IrTypeId* operands,size_t count)
{
    XL6_IrTypeId owner=operation==XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CREATE ? result : receiver;
    XL6_IrTypeId schemaId=XL6_IR_TYPE_INVALID;
    return xl6IrConcurrentQueueConstructionRecipe(module,owner,NULL,&schemaId,NULL) &&
        xl6IrConcurrentQueueLogicalSignature(module,operation,receiver,result,operands,count,schemaId,0);
}

/* Open templates carry a logical dependent contract, never a guessed slot.
 * Only an enclosing generic function can authorize it; specialization must
 * re-close the full resident schema before any executable instruction exists. */
static int xl6IrGenericConcurrentQueueSignatureCompatible(const XL6_IrModule* module,
    XL6_IrNodeId origin,XL6_IrRuntimeResourceOp operation,XL6_IrTypeId receiver,
    XL6_IrTypeId result,const XL6_IrTypeId* operands,size_t count)
{
    if (!module || !xl6IrRuntimeResourceOpIsSpecialQueue(operation)) return 0;
    XL6_IrTypeId ownerId=operation==XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CREATE ? result : receiver;
    const XL6_IrType* queue=xl6IrTypeGet(&module->types,ownerId);
    if (!queue || queue->kind!=XL6_IR_TYPE_GENERIC_INSTANCE ||
        !xl6IrRuntimeResourceAbiIsConcurrentQueue(queue->runtimeResourceAbi) ||
        queue->paramCount!=1 || !queue->params ||
        !xl6IrBuildTypeContainsTypeParam(module,queue->params[0],0) ||
        !queue->concurrentQueueSchemaType || queue->concurrentQueueSchemaType!=
            xl6IrConcurrentQueueSchemaTypeForType(&module->types,queue)) return 0;
    const XL6_IrNode* declaration=xl6IrNodeGet(module,origin);
    size_t depth=0;
    while (declaration && declaration->kind!=XL6_IR_NODE_FUNC_DECL && ++depth<=module->nodeCount)
        declaration=xl6IrNodeGet(module,declaration->parent);
    return declaration &&
        (xl6IrBuildTypeContainsTypeParam(module,declaration->declarationOwnerType,0) ||
         xl6IrBuildTypeContainsTypeParam(module,declaration->storageType,0)) &&
        xl6IrConcurrentQueueLogicalSignature(module,operation,receiver,result,operands,count,
            queue->concurrentQueueSchemaType,1);
}

static XL6_IrRuntimeResourceOp xl6IrConcurrentQueueResourceOperation(XL6_IrRuntimeOp op)
{
    switch (op) {
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CREATE;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_PUSH: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_TRY_PUSH;
    case XL6_IR_RUNTIME_CONCURRENT_QUEUE_TRY_POP: return XL6_IR_RUNTIME_RESOURCE_OP_CONCURRENT_QUEUE_TRY_POP;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_PUSH_BATCH: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_PUSH_BATCH;
    case XL6_IR_RUNTIME_CONCURRENT_QUEUE_POP_BATCH: return XL6_IR_RUNTIME_RESOURCE_OP_CONCURRENT_QUEUE_POP_BATCH;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CLOSE: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CLOSE;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_COUNT: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_COUNT;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CAPACITY: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_CAPACITY;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_RESET: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_RESET;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_CLOSED: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_IS_CLOSED;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_IS_DRAINED;
    /* Recognize invalid native legacy calls, so they never fall back to WaitQueue. */
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_POP_INTO: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_TRY_POP_INTO;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_INTO: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_POP_INTO;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_TIMEOUT_INTO: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_POP_TIMEOUT_INTO;
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_BATCH_INTO: return XL6_IR_RUNTIME_RESOURCE_OP_SPECIAL_QUEUE_POP_BATCH_INTO;
    default: return XL6_IR_RUNTIME_RESOURCE_OP_INVALID;
    }
}

XL6_IrTypeId xl6IrConcurrentQueueInstructionType(const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    if (!module || !instruction || instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
        !xl6IrConcurrentQueueResourceOperation(instruction->runtimeOp)) return XL6_IR_TYPE_INVALID;
    XL6_IrTypeId id=instruction->type;
    if (instruction->runtimeOp!=XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE) {
        if (!instruction->operandCount || instruction->firstOperand>=module->operandCount) return XL6_IR_TYPE_INVALID;
        const XL6_IrInstruction* receiver=xl6IrValueDefinition(module,module->operands[instruction->firstOperand],NULL);
        if (!receiver) return XL6_IR_TYPE_INVALID;
        id=receiver->type;
    }
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    return type && xl6IrRuntimeResourceAbiIsConcurrentQueue(type->runtimeResourceAbi) ? id : XL6_IR_TYPE_INVALID;
}

int xl6IrConcurrentQueueInstructionCompatible(const XL6_IrModule* module,const XL6_IrInstruction* instruction)
{
    XL6_IrTypeId owner=xl6IrConcurrentQueueInstructionType(module,instruction);
    XL6_IrTypeId schemaId=XL6_IR_TYPE_INVALID,operands[2];
    XL6_IrNominalIdentitySetId set=XL6_IR_NOMINAL_IDENTITY_SET_INVALID;
    if (!owner || !xl6IrConcurrentQueueConstructionRecipe(module,owner,NULL,&schemaId,&set) ||
        !instruction->result || instruction->operandCount>2 ||
        instruction->firstOperand>module->operandCount ||
        instruction->operandCount>module->operandCount-instruction->firstOperand ||
        instruction->immediateText || instruction->immediateData || instruction->immediateSize ||
        instruction->opcode!=XL6_IR_OPCODE_INVALID || instruction->targetValue ||
        instruction->targetSymbol || instruction->calleeType || instruction->runtimeIdentityType ||
        instruction->runtimeResourceKeyType || instruction->runtimeResourceKeyPayloadAbi ||
        instruction->runtimeResourceKeyOrderAbi) return 0;
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    int creates=instruction->runtimeOp==XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE;
    if (!schema || instruction->payloadType!=schema->sequenceLayout.elementType ||
        instruction->runtimeResourcePayloadAbi!=schema->sequenceLayout.transferAbi ||
        instruction->sequenceAdmissionIdentitySet!=(creates ? set : XL6_IR_NOMINAL_IDENTITY_SET_INVALID)) return 0;
    for (size_t i=0;i<instruction->operandCount;++i) {
        const XL6_IrInstruction* value=xl6IrValueDefinition(module,module->operands[instruction->firstOperand+i],NULL);
        if (!value) return 0;
        operands[i]=value->type;
    }
    XL6_IrTypeId result=instruction->type;
    if (instruction->runtimeOp==XL6_IR_RUNTIME_SPECIAL_QUEUE_CLOSE) {
        if (!xl6IrConcurrentQueueBoolType(module,result)) return 0;
        result=XL6_IR_TYPE_INVALID; /* Checked hidden status; source return is void. */
    }
    return xl6IrConcurrentQueueSignatureCompatible(module,
        xl6IrConcurrentQueueResourceOperation(instruction->runtimeOp),
        creates ? XL6_IR_TYPE_INVALID : owner,result,operands,instruction->operandCount);
}
