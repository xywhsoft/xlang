/* Included after canonical message construction and owning optional results.
 * Shared contract for the public Channel cutover. No AST chase, output
 * variable or payload-codec guess is permitted in this plan. */
static const XL6_IrChannelReceiveSpec xl6ChannelReceiveSpecs[]={
#define XL6_CHANNEL_RECEIVE_OP(Id,Name,Mode,Timed) \
    {XL6_IR_CHANNEL_RECEIVE_##Id,XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_RECEIVE_##Id, \
        XL6_IR_RUNTIME_CHANNEL_RECEIVE_##Id,Name,Mode,Timed},
#include "src/abi/xl6_channel_receive_ops.inc"
#undef XL6_CHANNEL_RECEIVE_OP
};

const XL6_IrChannelReceiveSpec* xl6IrChannelReceiveSpec(XL6_IrChannelReceiveKind kind)
{
    for (size_t i=0;i<sizeof(xl6ChannelReceiveSpecs)/sizeof(*xl6ChannelReceiveSpecs);++i)
        if (xl6ChannelReceiveSpecs[i].kind==kind) return &xl6ChannelReceiveSpecs[i];
    return NULL;
}
const XL6_IrChannelReceiveSpec* xl6IrChannelReceiveMethod(const char* name)
{
    for (size_t i=0;name && i<sizeof(xl6ChannelReceiveSpecs)/sizeof(*xl6ChannelReceiveSpecs);++i)
        if (!strcmp(name,xl6ChannelReceiveSpecs[i].name)) return &xl6ChannelReceiveSpecs[i];
    return NULL;
}
const XL6_IrChannelReceiveSpec* xl6IrChannelReceiveSpecResource(XL6_IrRuntimeResourceOp op)
{
    for (size_t i=0;i<sizeof(xl6ChannelReceiveSpecs)/sizeof(*xl6ChannelReceiveSpecs);++i)
        if (xl6ChannelReceiveSpecs[i].source==op) return &xl6ChannelReceiveSpecs[i];
    return NULL;
}
const XL6_IrChannelReceiveSpec* xl6IrChannelReceiveSpecRuntime(XL6_IrRuntimeOp op)
{
    for (size_t i=0;i<sizeof(xl6ChannelReceiveSpecs)/sizeof(*xl6ChannelReceiveSpecs);++i)
        if (xl6ChannelReceiveSpecs[i].runtime==op) return &xl6ChannelReceiveSpecs[i];
    return NULL;
}
XL6_IrCallErrorPhysicalAbi xl6IrChannelReceiveErrorPhysicalAbi(XL6_IrRuntimeOp op)
{
    return xl6IrChannelReceiveSpecRuntime(op) ? XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT :
        XL6_IR_CALL_ERROR_PHYSICAL_INVALID;
}
XL6_IrTypeId xl6IrChannelReceivePayloadType(const XL6_IrModule* module,XL6_IrTypeId owner)
{
    XL6_IrTypeId schema=0;
    if (xl6IrChannelConstructionRecipe(module,owner,&schema,NULL))
        return xl6IrTypeGet(&module->types,schema)->sequenceLayout.elementType;
    const XL6_IrType* type=module ? xl6IrTypeGet(&module->types,owner) : NULL;
    return type && type->kind==XL6_IR_TYPE_GENERIC_INSTANCE &&
        type->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_CHANNEL && type->paramCount==1 && type->params &&
        xl6IrBuildTypeContainsTypeParam(module,type->params[0],0) && type->messageSchemaType &&
        type->messageSchemaType==xl6IrMessageSchemaTypeForType(&module->types,type) ? type->params[0] : 0;
}
int xl6IrChannelReceivePlanBuild(const XL6_IrModule* module,XL6_IrChannelReceiveKind kind,
    XL6_IrTypeId owner,XL6_IrTypeId result,XL6_IrChannelReceivePlan* output)
{
    XL6_IrChannelReceivePlan plan={.spec=xl6IrChannelReceiveSpec(kind),
        .ownerType=owner,.resultType=result,.errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE,
        .errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT};
    if (!plan.spec || !xl6IrChannelConstructionRecipe(module,owner,&plan.schemaType,
        &plan.admissionIdentitySet)) return 0;
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,plan.schemaType);
    plan.elementType=schema->sequenceLayout.elementType;
    plan.transferAbi=schema->sequenceLayout.transferAbi;
    plan.slotAbi=schema->sequenceLayout.slotAbi;
    if (!xl6IrOptionalTryResultCompatible(module,result,plan.elementType,XL6_IR_C_ABI_I64)) return 0;
    const XL6_IrType* tuple=xl6IrTypeGet(&module->types,result);
    if (!xl6IrConcurrentQueueIntegerType(module,tuple->params[0])) return 0;
    plan.resultValueType=tuple->params[1];
    const XL6_IrType* value=xl6IrTypeGet(&module->types,plan.resultValueType);
    plan.valueTagged=plan.resultValueType!=plan.elementType && value->cAbi==XL6_IR_C_ABI_TAGGED_NULLABLE;
    if (output) *output=plan;
    return 1;
}
int xl6IrChannelReceiveSignatureCompatible(const XL6_IrModule* module,XL6_IrChannelReceiveKind kind,
    XL6_IrTypeId owner,XL6_IrTypeId result,const XL6_IrTypeId* args,size_t count)
{
    XL6_IrChannelReceivePlan plan={0};
    return xl6IrChannelReceivePlanBuild(module,kind,owner,result,&plan) && args &&
        count==(plan.spec->timed ? 2u : 1u) && args[0]==owner &&
        (!plan.spec->timed || xl6IrConcurrentQueueIntegerType(module,args[1]));
}

/* This contract deliberately has no transfer/slot ABI. Only the declaration
 * of an enclosing generic function can authorize a dependent operation; the
 * concrete clone must pass the closed plan before lowering can execute it. */
int xl6IrChannelReceiveDependentSignatureCompatible(const XL6_IrModule* module,XL6_IrNodeId origin,
    XL6_IrRuntimeResourceOp op,XL6_IrTypeId owner,XL6_IrTypeId result,
    const XL6_IrTypeId* args,size_t count)
{
    const XL6_IrChannelReceiveSpec* spec=xl6IrChannelReceiveSpecResource(op);
    XL6_IrTypeId element=xl6IrChannelReceivePayloadType(module,owner);
    const XL6_IrType* tuple=module ? xl6IrTypeGet(&module->types,result) : NULL;
    if (!spec || !element || !xl6IrBuildTypeContainsTypeParam(module,element,0) || !args ||
        count!=(spec->timed ? 2u : 1u) || args[0]!=owner ||
        (spec->timed && !xl6IrConcurrentQueueIntegerType(module,args[1])) ||
        !tuple || tuple->kind!=XL6_IR_TYPE_TUPLE || tuple->paramCount!=2 || !tuple->params ||
        tuple->returnCount || tuple->nullAbi!=XL6_IR_NULL_ABI_NONE ||
        !xl6IrConcurrentQueueIntegerType(module,tuple->params[0])) return 0;
    const XL6_IrType* payload=xl6IrTypeGet(&module->types,element);
    const XL6_IrType* value=xl6IrTypeGet(&module->types,tuple->params[1]);
    if (!payload || !value || !((tuple->params[1]==element && payload->kind==XL6_IR_TYPE_NULLABLE) ||
        (value->kind==XL6_IR_TYPE_NULLABLE && value->paramCount==1 && value->params && value->params[0]==element))) return 0;
    const XL6_IrNode* declaration=xl6IrNodeGet(module,origin); size_t depth=0;
    while (declaration && declaration->kind!=XL6_IR_NODE_FUNC_DECL && ++depth<=module->nodeCount)
        declaration=xl6IrNodeGet(module,declaration->parent);
    if (!declaration || declaration->kind!=XL6_IR_NODE_FUNC_DECL) return 0;
    if (xl6IrBuildTypeContainsTypeParam(module,declaration->declarationOwnerType,0)) return 1;
    /* Uncalled free-function declarations do not yet have an interned
     * callable storage type. Their explicit type-parameter declaration is
     * the authority; a caller's incidental signature must not decide it. */
    if (declaration->firstChild>module->childCount ||
        declaration->childCount>module->childCount-declaration->firstChild) return 0;
    for (size_t i=0;i<declaration->childCount;++i) {
        const XL6_IrNode* parameters=xl6IrNodeGet(module,module->children[declaration->firstChild+i]);
        if (parameters && parameters->sourceKind==XL6_AST_TYPE_PARAMS && parameters->childCount) return 1;
    }
    return 0;
}

int xl6IrChannelReceiveIntrinsicCompatible(const XL6_IrModule* module,const XL6_IrIntrinsicCall* call,
    const XL6_IrIntrinsicOperand* args)
{
    const XL6_IrChannelReceiveSpec* spec=call ? xl6IrChannelReceiveSpecResource(call->runtimeResourceOp) : NULL;
    XL6_IrTypeId types[2]={0}; XL6_IrChannelReceivePlan plan={0};
    if (!module || !spec || !args || call->kind!=XL6_IR_INTRINSIC_RUNTIME_RESOURCE ||
        call->admission!=XL6_IR_INTRINSIC_ADMISSION_VERIFIED || !call->receiver ||
        call->operandCount!=(spec->timed ? 2u : 1u) || args[0].expression!=call->receiver ||
        call->runtimeResourceKeyType || call->runtimeResourceKeyPayloadAbi || call->runtimeResourceKeyOrderAbi ||
        call->collectionAbi || call->integerTarget || call->payloadAbi ||
        !xl6IrChannelReceivePlanBuild(module,spec->kind,call->receiverType,call->resultType,&plan)) return 0;
    for (size_t i=0;i<call->operandCount;++i) {
        const XL6_IrType* type=xl6IrTypeGet(&module->types,args[i].type); types[i]=args[i].type;
        if (!type || !args[i].expression || args[i].abi!=(type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED ?
            XL6_IR_INTRINSIC_OPERAND_BORROW : XL6_IR_INTRINSIC_OPERAND_VALUE)) return 0;
    }
    return xl6IrChannelReceiveSignatureCompatible(module,spec->kind,plan.ownerType,plan.resultType,types,call->operandCount) &&
        call->payloadType==plan.elementType && call->runtimeResourcePayloadAbi==plan.transferAbi &&
        call->errorAbi==plan.errorAbi &&
        call->resultAbi==xl6IrRuntimeResourceResultAbiForType(spec->source,xl6IrTypeGet(&module->types,plan.resultType));
}

int xl6IrChannelReceiveInstructionPlan(const XL6_IrModule* module,const XL6_IrInstruction* ins,
    XL6_IrChannelReceivePlan* output)
{
    const XL6_IrChannelReceiveSpec* spec=ins ? xl6IrChannelReceiveSpecRuntime(ins->runtimeOp) : NULL;
    XL6_IrTypeId args[2]={0}; XL6_IrChannelReceivePlan plan={0};
    if (!module || !spec || ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !ins->result ||
        ins->operandCount!=(spec->timed ? 2u : 1u) || ins->firstOperand>module->operandCount ||
        ins->operandCount>module->operandCount-ins->firstOperand ||
        ins->immediateText || ins->immediateData || ins->immediateSize || ins->opcode || ins->targetSymbol ||
        ins->targetValue || ins->calleeType || ins->runtimeIdentityType || ins->sequenceAdmissionIdentitySet ||
        ins->runtimeResourceKeyType || ins->runtimeResourceKeyPayloadAbi || ins->runtimeResourceKeyOrderAbi ||
        ins->collectionAbi) return 0;
    for (size_t i=0;i<ins->operandCount;++i) {
        const XL6_IrInstruction* def=xl6IrValueDefinition(module,module->operands[ins->firstOperand+i],NULL);
        if (!def) return 0;
        args[i]=def->type;
    }
    if (!xl6IrChannelReceiveSignatureCompatible(module,spec->kind,args[0],ins->type,args,ins->operandCount) ||
        !xl6IrChannelReceivePlanBuild(module,spec->kind,args[0],ins->type,&plan) ||
        ins->payloadType!=plan.elementType || ins->runtimeResourcePayloadAbi!=plan.transferAbi) return 0;
    if (output) *output=plan;
    return 1;
}

int xl6IrChannelRuntimeOpIs(XL6_IrRuntimeOp op)
{
    return (op>=XL6_IR_RUNTIME_CHANNEL_CREATE && op<=XL6_IR_RUNTIME_CHANNEL_COUNT) ||
        xl6IrChannelReceiveSpecRuntime(op)!=NULL;
}
XL6_IrCallErrorPhysicalAbi xl6IrChannelInstructionErrorPhysicalAbi(XL6_IrRuntimeOp op)
{
    return xl6IrChannelRuntimeOpIs(op) && op!=XL6_IR_RUNTIME_CHANNEL_RECV_INTO &&
        op!=XL6_IR_RUNTIME_CHANNEL_TRY_RECV_INTO && op!=XL6_IR_RUNTIME_CHANNEL_RECV_TIMEOUT_INTO ?
        XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT : XL6_IR_CALL_ERROR_PHYSICAL_INVALID;
}
XL6_IrTypeId xl6IrChannelInstructionType(const XL6_IrModule* module,const XL6_IrInstruction* ins)
{
    if (!module || !ins || ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !xl6IrChannelRuntimeOpIs(ins->runtimeOp)) return 0;
    if (ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CREATE) return ins->type;
    if (!ins->operandCount || ins->firstOperand>=module->operandCount) return 0;
    const XL6_IrInstruction* receiver=xl6IrValueDefinition(module,module->operands[ins->firstOperand],NULL);
    return receiver ? receiver->type : 0;
}
int xl6IrChannelInstructionCompatible(const XL6_IrModule* module,const XL6_IrInstruction* ins)
{
    if (!module || !ins || !xl6IrChannelInstructionErrorPhysicalAbi(ins->runtimeOp)) return 0;
    if (xl6IrChannelReceiveSpecRuntime(ins->runtimeOp)) return xl6IrChannelReceiveInstructionPlan(module,ins,NULL);
    XL6_IrTypeId owner=xl6IrChannelInstructionType(module,ins),schemaId=0;
    XL6_IrNominalIdentitySetId set=0;
    if (!owner || !xl6IrChannelConstructionRecipe(module,owner,&schemaId,&set) || !ins->result ||
        ins->firstOperand>module->operandCount || ins->operandCount>module->operandCount-ins->firstOperand ||
        ins->immediateText || ins->immediateData || ins->immediateSize || ins->opcode || ins->targetSymbol ||
        ins->targetValue || ins->calleeType || ins->runtimeIdentityType || ins->collectionAbi ||
        ins->runtimeResourceKeyType || ins->runtimeResourceKeyPayloadAbi || ins->runtimeResourceKeyOrderAbi) return 0;
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    const XL6_IrType* result=xl6IrTypeGet(&module->types,ins->type);
    if (!result || ins->payloadType!=schema->sequenceLayout.elementType ||
        ins->runtimeResourcePayloadAbi!=schema->sequenceLayout.transferAbi ||
        ins->sequenceAdmissionIdentitySet!=(ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CREATE ? set : 0)) return 0;
    if (ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CREATE) {
        const XL6_IrInstruction* capacity=ins->operandCount==1 ?
            xl6IrValueDefinition(module,module->operands[ins->firstOperand],NULL) : NULL;
        return ins->operandCount<=1 && (!ins->operandCount || (capacity && xl6IrConcurrentQueueIntegerType(module,capacity->type)));
    }
    if (ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_COUNT)
        return ins->operandCount==1 && xl6IrConcurrentQueueIntegerType(module,ins->type);
    if (result->kind!=XL6_IR_TYPE_BUILTIN || result->cAbi!=XL6_IR_C_ABI_BOOL ||
        result->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL || result->nullAbi!=XL6_IR_NULL_ABI_NONE) return 0;
    if (ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_SEND || ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_TRY_SEND) {
        const XL6_IrInstruction* value=ins->operandCount==2 ?
            xl6IrValueDefinition(module,module->operands[ins->firstOperand+1],NULL) : NULL;
        return value && value->type==ins->payloadType;
    }
    return ins->operandCount==1 && (ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CLOSE || ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_IS_CLOSED);
}
