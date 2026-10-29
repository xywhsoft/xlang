/* Included after canonical message construction and owning optional results.
 * Shared contract for the public Channel cutover. No AST chase, output
 * variable or payload-codec guess is permitted in this plan. */
static int xl6IrChannelDependentDeclaration(const XL6_IrModule* module,XL6_IrNodeId origin)
{
    const XL6_IrNode* declaration=xl6IrNodeGet(module,origin); size_t depth=0;
    while (declaration && declaration->kind!=XL6_IR_NODE_FUNC_DECL && ++depth<=module->nodeCount)
        declaration=xl6IrNodeGet(module,declaration->parent);
    if (!declaration || declaration->kind!=XL6_IR_NODE_FUNC_DECL) return 0;
    if (xl6IrBuildTypeContainsTypeParam(module,declaration->declarationOwnerType,0)) return 1;
    /* Uncalled generic free functions have no interned callable storage yet.
     * Their declaration, not an incidental caller type, is the authority. */
    if (declaration->firstChild>module->childCount ||
        declaration->childCount>module->childCount-declaration->firstChild) return 0;
    for (size_t i=0;i<declaration->childCount;++i) {
        const XL6_IrNode* parameters=xl6IrNodeGet(module,module->children[declaration->firstChild+i]);
        if (parameters && parameters->sourceKind==XL6_AST_TYPE_PARAMS && parameters->childCount) return 1;
    }
    return 0;
}
static int xl6IrChannelControlDependentSignatureCompatible(const XL6_IrModule* module,
    XL6_IrNodeId origin,XL6_IrRuntimeResourceOp op,XL6_IrTypeId owner,
    XL6_IrTypeId result,const XL6_IrTypeId* args,size_t count)
{
    const XL6_IrType* type=module?xl6IrTypeGet(&module->types,owner):NULL;
    const XL6_IrType* output=module?xl6IrTypeGet(&module->types,result):NULL;
    if (!type || !args || count!=1 || args[0]!=owner ||
        type->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || type->paramCount!=1 || !type->params ||
        type->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_CHANNEL ||
        !xl6IrBuildTypeContainsTypeParam(module,type->params[0],0) || !type->messageSchemaType ||
        type->messageSchemaType!=xl6IrMessageSchemaTypeForType(&module->types,type) ||
        !xl6IrChannelDependentDeclaration(module,origin)) return 0;
    if (op==XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_CLOSE) return result==XL6_IR_TYPE_INVALID;
    if (!output) return 0;
    if (op==XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_COUNT)
        return output->kind==XL6_IR_TYPE_BUILTIN && output->cAbi==XL6_IR_C_ABI_I64 &&
            output->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64 && output->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
            output->nullAbi==XL6_IR_NULL_ABI_NONE;
    return op==XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_IS_CLOSED &&
        output->kind==XL6_IR_TYPE_BUILTIN && output->cAbi==XL6_IR_C_ABI_BOOL &&
        output->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL && output->nullAbi==XL6_IR_NULL_ABI_NONE;
}
/* Relative time is a checked signed millisecond span. An absolute deadline
 * uses the unsigned millisecond clock exposed by time.monotonic(). Neither
 * semantic acceptance nor a generic clone may silently change the unit. */
static int xl6IrChannelWaitTimeCompatible(const XL6_IrModule* module,unsigned mode,XL6_IrTypeId id)
{
    if (mode!=3) return xl6IrConcurrentQueueIntegerType(module,id);
    const XL6_IrType* type=module ? xl6IrTypeGet(&module->types,id) : NULL;
    return type && type->kind==XL6_IR_TYPE_BUILTIN && type->cAbi==XL6_IR_C_ABI_U64 &&
        type->integerTarget==XL6_IR_INTEGER_TARGET_UNSIGNED_64 &&
        type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL && type->nullAbi==XL6_IR_NULL_ABI_NONE;
}
const XL6_IrClassField* xl6IrChannelCancellationField(const XL6_IrModule* module,XL6_IrTypeId type)
{
    return xl6IrResidentNativeResourceField(module,type,"xl6CancellationTokenDestroyV1");
}
static size_t xl6IrChannelSendCount(const XL6_IrChannelSendSpec* spec)
{ return 2u+(spec->timed ? 1u : 0u)+(spec->cancelled ? 1u : 0u); }
static size_t xl6IrChannelReceiveCount(const XL6_IrChannelReceiveSpec* spec)
{ return 1u+(spec->timed ? 1u : 0u)+(spec->cancelled ? 1u : 0u); }
static int xl6IrChannelSendResultCompatible(const XL6_IrModule* module,
    const XL6_IrChannelSendSpec* spec,XL6_IrTypeId id)
{
    const XL6_IrType* result=module ? xl6IrTypeGet(&module->types,id) : NULL;
    return spec->cancelled ? xl6IrConcurrentQueueIntegerType(module,id) :
        result && result->kind==XL6_IR_TYPE_BUILTIN && result->cAbi==XL6_IR_C_ABI_BOOL &&
        result->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL && result->nullAbi==XL6_IR_NULL_ABI_NONE;
}
static const XL6_IrChannelSendSpec xl6ChannelSendSpecs[]={
#define XL6_CHANNEL_SEND_OP(Id,Name,Mode,Timed,Cancelled,Label) \
    {XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_##Id,XL6_IR_RUNTIME_CHANNEL_##Id,Name,Mode,Timed,Cancelled, \
        Cancelled ? "xl6ChannelSendCancelV2" : Mode==3 ? "xl6ChannelSendWaitV2" : "xl6ChannelSendV1"},
#include "src/abi/xl6_channel_send_ops.inc"
#undef XL6_CHANNEL_SEND_OP
};
const XL6_IrChannelSendSpec* xl6IrChannelSendMethod(const char* name)
{
    for (size_t i=0;name && i<sizeof(xl6ChannelSendSpecs)/sizeof(*xl6ChannelSendSpecs);++i)
        if (!strcmp(name,xl6ChannelSendSpecs[i].name)) return &xl6ChannelSendSpecs[i];
    return NULL;
}
const XL6_IrChannelSendSpec* xl6IrChannelSendSpecResource(XL6_IrRuntimeResourceOp op)
{
    for (size_t i=0;i<sizeof(xl6ChannelSendSpecs)/sizeof(*xl6ChannelSendSpecs);++i)
        if (op==xl6ChannelSendSpecs[i].source) return &xl6ChannelSendSpecs[i];
    return NULL;
}
const XL6_IrChannelSendSpec* xl6IrChannelSendSpecRuntime(XL6_IrRuntimeOp op)
{
    for (size_t i=0;i<sizeof(xl6ChannelSendSpecs)/sizeof(*xl6ChannelSendSpecs);++i)
        if (op==xl6ChannelSendSpecs[i].runtime) return &xl6ChannelSendSpecs[i];
    return NULL;
}
int xl6IrChannelSendSignatureCompatible(const XL6_IrModule* module,const XL6_IrChannelSendSpec* spec,
    XL6_IrTypeId owner,XL6_IrTypeId result,const XL6_IrTypeId* args,size_t count)
{
    XL6_IrTypeId schemaId=0;
    if (!spec || spec!=xl6IrChannelSendSpecResource(spec->source) || !args ||
        count!=xl6IrChannelSendCount(spec) || args[0]!=owner ||
        !xl6IrChannelConstructionRecipe(module,owner,&schemaId,NULL) ||
        !xl6IrChannelSendResultCompatible(module,spec,result)) return 0;
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    return args[1]==schema->sequenceLayout.elementType &&
        (!spec->timed || xl6IrChannelWaitTimeCompatible(module,spec->waitMode,args[2])) &&
        (!spec->cancelled || xl6IrChannelCancellationField(module,args[count-1]));
}
int xl6IrChannelSendIntrinsicCompatible(const XL6_IrModule* module,const XL6_IrIntrinsicCall* call,
    const XL6_IrIntrinsicOperand* args)
{
    const XL6_IrChannelSendSpec* spec=call ? xl6IrChannelSendSpecResource(call->runtimeResourceOp) : NULL;
    XL6_IrTypeId types[4]={0},schemaId=0;
    if (!module || !spec || !args || call->kind!=XL6_IR_INTRINSIC_RUNTIME_RESOURCE ||
        call->admission!=XL6_IR_INTRINSIC_ADMISSION_VERIFIED || !call->receiver ||
        call->operandCount!=xl6IrChannelSendCount(spec) || args[0].expression!=call->receiver ||
        call->runtimeResourceKeyType || call->runtimeResourceKeyPayloadAbi || call->runtimeResourceKeyOrderAbi ||
        call->collectionAbi || call->integerTarget || call->payloadAbi ||
        call->errorAbi!=XL6_IR_ERROR_ABI_MAY_RAISE || call->resultAbi!=XL6_IR_INTRINSIC_RESULT_VALUE ||
        !xl6IrChannelConstructionRecipe(module,call->receiverType,&schemaId,NULL)) return 0;
    const XL6_IrType* schema=xl6IrTypeGet(&module->types,schemaId);
    if (call->payloadType!=schema->sequenceLayout.elementType ||
        call->runtimeResourcePayloadAbi!=schema->sequenceLayout.transferAbi) return 0;
    for (size_t i=0;i<call->operandCount;++i) {
        const XL6_IrType* type=xl6IrTypeGet(&module->types,args[i].type); types[i]=args[i].type;
        if (!type || !args[i].expression || args[i].abi!=(type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED ?
            XL6_IR_INTRINSIC_OPERAND_BORROW : XL6_IR_INTRINSIC_OPERAND_VALUE)) return 0;
    }
    return xl6IrChannelSendSignatureCompatible(module,spec,call->receiverType,call->resultType,types,call->operandCount);
}
int xl6IrChannelSendDependentSignatureCompatible(const XL6_IrModule* module,XL6_IrNodeId origin,
    XL6_IrRuntimeResourceOp op,XL6_IrTypeId owner,XL6_IrTypeId result,const XL6_IrTypeId* args,size_t count)
{
    const XL6_IrChannelSendSpec* spec=xl6IrChannelSendSpecResource(op);
    XL6_IrTypeId payload=xl6IrChannelReceivePayloadType(module,owner);
    return spec && payload && xl6IrBuildTypeContainsTypeParam(module,payload,0) && args &&
        count==xl6IrChannelSendCount(spec) && args[0]==owner && args[1]==payload &&
        (!spec->timed || xl6IrChannelWaitTimeCompatible(module,spec->waitMode,args[2])) &&
        (!spec->cancelled || xl6IrChannelCancellationField(module,args[count-1])) &&
        xl6IrChannelSendResultCompatible(module,spec,result) &&
        xl6IrChannelDependentDeclaration(module,origin);
}
static const XL6_IrChannelReceiveSpec xl6ChannelReceiveSpecs[]={
#define XL6_CHANNEL_RECEIVE_OP(Id,Name,Mode,Timed,Cancelled) \
    {XL6_IR_CHANNEL_RECEIVE_##Id,XL6_IR_RUNTIME_RESOURCE_OP_CHANNEL_RECEIVE_##Id, \
        XL6_IR_RUNTIME_CHANNEL_RECEIVE_##Id,Name,Mode,Timed,Cancelled, \
        Cancelled ? "xl6ChannelReceiveCancelV2" : Mode==3 ? "xl6ChannelReceiveWaitV2" : "xl6ChannelReceiveV1"},
#include "src/abi/xl6_channel_receive_ops.inc"
#undef XL6_CHANNEL_RECEIVE_OP
};

const XL6_IrChannelReceiveSpec* xl6IrChannelReceiveSpec(XL6_IrChannelReceiveKind kind)
{
    for (size_t i=0;i<sizeof(xl6ChannelReceiveSpecs)/sizeof(*xl6ChannelReceiveSpecs);++i)
        if (xl6ChannelReceiveSpecs[i].kind==kind) return &xl6ChannelReceiveSpecs[i];
    return NULL;
}
int xl6IrChannelCancellationOperand(XL6_IrRuntimeResourceOp op,size_t index)
{
    const XL6_IrChannelSendSpec* send=xl6IrChannelSendSpecResource(op);
    const XL6_IrChannelReceiveSpec* receive=xl6IrChannelReceiveSpecResource(op);
    return (send && send->cancelled && index+1==xl6IrChannelSendCount(send)) ||
        (receive && receive->cancelled && index+1==xl6IrChannelReceiveCount(receive));
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
    plan.valueTagged=xl6IrOptionalResultPayloadTagged(module,plan.resultValueType,plan.elementType);
    if (output) *output=plan;
    return 1;
}
int xl6IrChannelReceiveSignatureCompatible(const XL6_IrModule* module,XL6_IrChannelReceiveKind kind,
    XL6_IrTypeId owner,XL6_IrTypeId result,const XL6_IrTypeId* args,size_t count)
{
    XL6_IrChannelReceivePlan plan={0};
    return xl6IrChannelReceivePlanBuild(module,kind,owner,result,&plan) && args &&
        count==xl6IrChannelReceiveCount(plan.spec) && args[0]==owner &&
        (!plan.spec->timed || xl6IrChannelWaitTimeCompatible(module,plan.spec->waitMode,args[1])) &&
        (!plan.spec->cancelled || xl6IrChannelCancellationField(module,args[count-1]));
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
        count!=xl6IrChannelReceiveCount(spec) || args[0]!=owner ||
        (spec->timed && !xl6IrChannelWaitTimeCompatible(module,spec->waitMode,args[1])) ||
        (spec->cancelled && !xl6IrChannelCancellationField(module,args[count-1])) ||
        !tuple || tuple->kind!=XL6_IR_TYPE_TUPLE || tuple->paramCount!=2 || !tuple->params ||
        tuple->returnCount || tuple->nullAbi!=XL6_IR_NULL_ABI_NONE ||
        !xl6IrConcurrentQueueIntegerType(module,tuple->params[0])) return 0;
    const XL6_IrType* payload=xl6IrTypeGet(&module->types,element);
    const XL6_IrType* value=xl6IrTypeGet(&module->types,tuple->params[1]);
    if (!payload || !value || !((tuple->params[1]==element && payload->kind==XL6_IR_TYPE_NULLABLE) ||
        (value->kind==XL6_IR_TYPE_NULLABLE && value->paramCount==1 && value->params && value->params[0]==element))) return 0;
    return xl6IrChannelDependentDeclaration(module,origin);
}

int xl6IrChannelReceiveIntrinsicCompatible(const XL6_IrModule* module,const XL6_IrIntrinsicCall* call,
    const XL6_IrIntrinsicOperand* args)
{
    const XL6_IrChannelReceiveSpec* spec=call ? xl6IrChannelReceiveSpecResource(call->runtimeResourceOp) : NULL;
    XL6_IrTypeId types[3]={0}; XL6_IrChannelReceivePlan plan={0};
    if (!module || !spec || !args || call->kind!=XL6_IR_INTRINSIC_RUNTIME_RESOURCE ||
        call->admission!=XL6_IR_INTRINSIC_ADMISSION_VERIFIED || !call->receiver ||
        call->operandCount!=xl6IrChannelReceiveCount(spec) || args[0].expression!=call->receiver ||
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

/* A private physical loan has exactly one use: the final Token input of this
 * verified synchronous wait. It cannot be stored, returned, or reused as an
 * arbitrary point value. The logical object remains the borrowed owner. */
int xl6IrChannelCancellationProjectionCompatible(const XL6_IrModule* module,const XL6_IrInstruction* getter)
{
    const XL6_IrIntrinsicCall* call=module && getter ? xl6IrIntrinsicCallGet(module,getter->origin) : NULL;
    const XL6_IrChannelSendSpec* send=call ? xl6IrChannelSendSpecResource(call->runtimeResourceOp) : NULL;
    const XL6_IrChannelReceiveSpec* receive=call ? xl6IrChannelReceiveSpecResource(call->runtimeResourceOp) : NULL;
    if (!call || !getter->result || getter->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
        getter->runtimeOp!=XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW ||
        getter->operandCount!=1 || getter->firstOperand>=module->operandCount ||
        getter->immediateData || getter->immediateSize || getter->opcode || getter->targetSymbol ||
        getter->targetValue || getter->calleeType || getter->payloadType || getter->runtimeIdentityType ||
        getter->runtimeResourcePayloadAbi || getter->runtimeResourceKeyType ||
        getter->runtimeResourceKeyPayloadAbi || getter->runtimeResourceKeyOrderAbi ||
        getter->sequenceAdmissionIdentitySet || getter->collectionAbi || getter->integerTarget ||
        getter->integerSourceAbi || getter->scalarConversionAbi || getter->castAbi ||
        !((send && send->cancelled) || (receive && receive->cancelled)) ||
        call->firstOperand>module->intrinsicOperandCount ||
        call->operandCount>module->intrinsicOperandCount-call->firstOperand || !call->operandCount) return 0;
    const XL6_IrIntrinsicOperand* args=module->intrinsicOperands+call->firstOperand;
    if (!(send ? xl6IrChannelSendIntrinsicCompatible(module,call,args) :
        xl6IrChannelReceiveIntrinsicCompatible(module,call,args))) return 0;
    const XL6_IrInstruction* owner=xl6IrValueDefinition(module,module->operands[getter->firstOperand],NULL);
    const XL6_IrClassField* field=owner ? xl6IrChannelCancellationField(module,owner->type) : NULL;
    if (!field || owner->type!=args[call->operandCount-1].type || getter->type!=field->type ||
        !xl6IrCanonicalCStringLiteralMatchesText(getter->immediateText,field->name)) return 0;
    size_t uses=0;
    for (size_t i=0;i<module->instructionCount;++i) {
        const XL6_IrInstruction* use=&module->instructions[i];
        if (use->targetValue==getter->result || use->firstOperand>module->operandCount ||
            use->operandCount>module->operandCount-use->firstOperand) return 0;
        for (size_t j=0;j<use->operandCount;++j) if (module->operands[use->firstOperand+j]==getter->result) {
            if (use->kind!=XL6_IR_INSTR_RUNTIME_CALL || use->origin!=getter->origin ||
                use->runtimeOp!=(send ? send->runtime : receive->runtime) ||
                use->operandCount!=call->operandCount || j+1!=use->operandCount) return 0;
            ++uses;
        }
    }
    return uses==1;
}
static int xl6IrChannelPhysicalArgumentTypes(const XL6_IrModule* module,const XL6_IrInstruction* ins,
    bool cancelled,XL6_IrTypeId* args)
{
    for (size_t i=0;i<ins->operandCount;++i) {
        const XL6_IrInstruction* def=xl6IrValueDefinition(module,module->operands[ins->firstOperand+i],NULL);
        if (!def) return 0;
        args[i]=def->type;
        if (cancelled && i+1==ins->operandCount) {
            if (def->origin!=ins->origin || !xl6IrChannelCancellationProjectionCompatible(module,def)) return 0;
            args[i]=xl6IrValueDefinition(module,module->operands[def->firstOperand],NULL)->type;
        }
    }
    return 1;
}
int xl6IrChannelReceiveInstructionPlan(const XL6_IrModule* module,const XL6_IrInstruction* ins,
    XL6_IrChannelReceivePlan* output)
{
    const XL6_IrChannelReceiveSpec* spec=ins ? xl6IrChannelReceiveSpecRuntime(ins->runtimeOp) : NULL;
    XL6_IrTypeId args[3]={0}; XL6_IrChannelReceivePlan plan={0};
    if (!module || !spec || ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !ins->result ||
        ins->operandCount!=xl6IrChannelReceiveCount(spec) || ins->firstOperand>module->operandCount ||
        ins->operandCount>module->operandCount-ins->firstOperand ||
        ins->immediateText || ins->immediateData || ins->immediateSize || ins->opcode || ins->targetSymbol ||
        ins->targetValue || ins->calleeType || ins->runtimeIdentityType || ins->sequenceAdmissionIdentitySet ||
        ins->runtimeResourceKeyType || ins->runtimeResourceKeyPayloadAbi || ins->runtimeResourceKeyOrderAbi ||
        ins->collectionAbi) return 0;
    if (!xl6IrChannelPhysicalArgumentTypes(module,ins,spec->cancelled,args)) return 0;
    if (!xl6IrChannelReceiveSignatureCompatible(module,spec->kind,args[0],ins->type,args,ins->operandCount) ||
        !xl6IrChannelReceivePlanBuild(module,spec->kind,args[0],ins->type,&plan) ||
        ins->payloadType!=plan.elementType || ins->runtimeResourcePayloadAbi!=plan.transferAbi) return 0;
    if (output) *output=plan;
    return 1;
}

int xl6IrChannelRuntimeOpIs(XL6_IrRuntimeOp op)
{
    return (op>=XL6_IR_RUNTIME_CHANNEL_CREATE && op<=XL6_IR_RUNTIME_CHANNEL_COUNT) ||
        xl6IrChannelReceiveSpecRuntime(op)!=NULL || xl6IrChannelSendSpecRuntime(op)!=NULL;
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
    const XL6_IrChannelSendSpec* send=xl6IrChannelSendSpecRuntime(ins->runtimeOp);
    if (send) {
        XL6_IrTypeId args[4]={0};
        if (ins->operandCount!=xl6IrChannelSendCount(send) ||
            !xl6IrChannelPhysicalArgumentTypes(module,ins,send->cancelled,args)) return 0;
        return xl6IrChannelSendSignatureCompatible(module,send,owner,ins->type,args,ins->operandCount);
    }
    if (result->kind!=XL6_IR_TYPE_BUILTIN || result->cAbi!=XL6_IR_C_ABI_BOOL ||
        result->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL || result->nullAbi!=XL6_IR_NULL_ABI_NONE) return 0;
    return ins->operandCount==1 && (ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_CLOSE || ins->runtimeOp==XL6_IR_RUNTIME_CHANNEL_IS_CLOSED);
}
