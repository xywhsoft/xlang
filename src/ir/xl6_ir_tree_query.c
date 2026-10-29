/* Included after the shared optional-result and Tree construction contracts.
 * Source signature, lowering, verifier and Cgen consume this one plan. */
static const XL6_IrTreeQuerySpec xl6TreeQuerySpecs[]={
#define XL6_TREE_QUERY_OP(id,name,search,pair,required,native) \
    {XL6_IR_RUNTIME_RESOURCE_OP_TREE_QUERY_##id,XL6_IR_RUNTIME_TREE_QUERY_##id, \
    name,"XL6_TREE_QUERY_" #native "_V2",search,pair,required},
#include "src/abi/xl6_tree_query_ops.inc"
#undef XL6_TREE_QUERY_OP
};
const XL6_IrTreeQuerySpec* xl6IrTreeQuerySpecResource(XL6_IrRuntimeResourceOp op)
{
    for(size_t i=0;i<sizeof(xl6TreeQuerySpecs)/sizeof(*xl6TreeQuerySpecs);++i)
        if(xl6TreeQuerySpecs[i].source==op) return &xl6TreeQuerySpecs[i];
    return NULL;
}
const XL6_IrTreeQuerySpec* xl6IrTreeQuerySpecRuntime(XL6_IrRuntimeOp op)
{
    for(size_t i=0;i<sizeof(xl6TreeQuerySpecs)/sizeof(*xl6TreeQuerySpecs);++i)
        if(xl6TreeQuerySpecs[i].runtime==op) return &xl6TreeQuerySpecs[i];
    return NULL;
}
XL6_IrRuntimeResourceOp xl6IrTreeQueryMethod(const char* name)
{
    for(size_t i=0;name && i<sizeof(xl6TreeQuerySpecs)/sizeof(*xl6TreeQuerySpecs);++i)
        if(strcmp(name,xl6TreeQuerySpecs[i].name)==0) return xl6TreeQuerySpecs[i].source;
    return XL6_IR_RUNTIME_RESOURCE_OP_INVALID;
}
XL6_IrCallErrorPhysicalAbi xl6IrTreeQueryErrorPhysicalAbi(XL6_IrRuntimeOp op)
{ return xl6IrTreeQuerySpecRuntime(op) ? XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT : XL6_IR_CALL_ERROR_PHYSICAL_INVALID; }
/* Generic declarations carry logical signatures, not guessed physical codecs.
 * Specialization must rebuild the closed plan before lowering and Cgen. */
static int xl6IrGenericTreeQuerySignatureCompatible(const XL6_IrModule* m,
    XL6_IrNodeId origin,XL6_IrRuntimeResourceOp op,XL6_IrTypeId ownerType,
    XL6_IrTypeId resultId,const XL6_IrTypeId* args,size_t count)
{
    const XL6_IrTreeQuerySpec* spec=xl6IrTreeQuerySpecResource(op);
    const XL6_IrType* tree=m ? xl6IrTypeGet(&m->types,ownerType) : NULL;
    const XL6_IrType* result=m ? xl6IrTypeGet(&m->types,resultId) : NULL;
    if(!spec || !tree || tree->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_AVL_TREE ||
        tree->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || tree->paramCount!=2 || !tree->params ||
        !xl6IrBuildTypeContainsTypeParam(m,ownerType,0) || !result || !args ||
        count!=(spec->search ? 2u : 1u) || args[0]!=ownerType ||
        (spec->search && args[1]!=tree->params[0])) return 0;
    const XL6_IrNode* owner=xl6IrNodeGet(m,origin); size_t depth=0;
    while(owner && owner->kind!=XL6_IR_NODE_FUNC_DECL && ++depth<=m->nodeCount)
        owner=xl6IrNodeGet(m,owner->parent);
    if(!owner || (!xl6IrBuildTypeContainsTypeParam(m,owner->declarationOwnerType,0) &&
        !xl6IrBuildTypeContainsTypeParam(m,owner->storageType,0))) return 0;
    if(spec->required && !spec->pair) return resultId==tree->params[1];
    size_t fields=(spec->pair ? 2u : 1u)+(spec->required ? 0u : 1u);
    if(result->kind!=XL6_IR_TYPE_TUPLE || result->paramCount!=fields || !result->params) return 0;
    if(!spec->required) {
        const XL6_IrType* found=xl6IrTypeGet(&m->types,result->params[0]);
        if(!found || found->kind!=XL6_IR_TYPE_BUILTIN || found->cAbi!=XL6_IR_C_ABI_BOOL) return 0;
    }
    for(size_t i=spec->pair ? 0u : 1u;i<2u;++i) {
        XL6_IrTypeId base=tree->params[i];
        size_t index=spec->required ? i : spec->pair ? i+1u : 1u;
        XL6_IrTypeId actual=result->params[index];
        const XL6_IrType* item=xl6IrTypeGet(&m->types,actual);
        const XL6_IrType* payload=xl6IrTypeGet(&m->types,base);
        if(spec->required) { if(actual!=base) return 0; }
        else if(!item || !payload || !((actual==base && payload->kind==XL6_IR_TYPE_NULLABLE) ||
            (item->kind==XL6_IR_TYPE_NULLABLE && item->paramCount==1 && item->params && item->params[0]==base))) return 0;
    }
    return 1;
}
int xl6IrTreeQueryPlanBuild(const XL6_IrModule* m,XL6_IrRuntimeResourceOp op,
    XL6_IrTypeId owner,XL6_IrTypeId resultId,XL6_IrTreeQueryPlan* output)
{
    const XL6_IrTreeQuerySpec* spec=xl6IrTreeQuerySpecResource(op);
    const XL6_IrType* tree=m ? xl6IrTypeGet(&m->types,owner) : NULL;
    const XL6_IrType* result=m ? xl6IrTypeGet(&m->types,resultId) : NULL;
    XL6_IrTreeQueryPlan p={.spec=spec,.ownerType=owner,.resultType=resultId};
    if(!spec || !tree || !result || !xl6IrAvlConstructionRecipe(m,owner,
        &p.keySchemaType,&p.valueSchemaType,NULL,NULL)) return 0;
    p.keyType=tree->params[0]; p.valueType=tree->params[1];
    p.product=spec->pair || !spec->required;
    if(p.product) {
        size_t count=(spec->pair ? 2u : 1u)+(spec->required ? 0u : 1u);
        if(result->kind!=XL6_IR_TYPE_TUPLE || result->cAbi!=XL6_IR_C_ABI_PRODUCT ||
            result->paramCount!=count || !result->params || result->returnCount ||
            result->nullAbi!=XL6_IR_NULL_ABI_NONE) return 0;
        if(!spec->required) {
            const XL6_IrType* found=xl6IrTypeGet(&m->types,result->params[0]);
            if(!found || found->kind!=XL6_IR_TYPE_BUILTIN || found->cAbi!=XL6_IR_C_ABI_BOOL ||
                found->nullAbi!=XL6_IR_NULL_ABI_NONE || found->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
                found->paramCount || found->returnCount) return 0;
        }
        p.keyIndex=spec->required ? 0u : 1u;
        p.valueIndex=spec->pair ? p.keyIndex+1u : 1u;
        p.resultKeyType=spec->pair ? result->params[p.keyIndex] : 0;
        p.resultValueType=result->params[p.valueIndex];
    } else p.resultValueType=resultId;
    if(spec->required) {
        if(p.resultValueType!=p.valueType || (spec->pair && p.resultKeyType!=p.keyType)) return 0;
    } else if(!xl6IrOptionalOwnedResultCompatible(m,p.resultValueType,p.valueType) ||
        (spec->pair && !xl6IrOptionalOwnedResultCompatible(m,p.resultKeyType,p.keyType))) return 0;
    p.valueTagged=p.resultValueType!=p.valueType &&
        xl6IrTypeGet(&m->types,p.resultValueType)->cAbi==XL6_IR_C_ABI_TAGGED_NULLABLE;
    p.keyTagged=spec->pair && p.resultKeyType!=p.keyType &&
        xl6IrTypeGet(&m->types,p.resultKeyType)->cAbi==XL6_IR_C_ABI_TAGGED_NULLABLE;
    if(output) *output=p;
    return 1;
}
int xl6IrTreeQuerySignatureCompatible(const XL6_IrModule* m,XL6_IrRuntimeResourceOp op,
    XL6_IrTypeId owner,XL6_IrTypeId result,const XL6_IrTypeId* args,size_t count)
{
    XL6_IrTreeQueryPlan p={0};
    return xl6IrTreeQueryPlanBuild(m,op,owner,result,&p) &&
        count==(p.spec->search ? 2u : 1u) && args && args[0]==owner &&
        (!p.spec->search || args[1]==p.keyType);
}
int xl6IrTreeQueryIntrinsicCompatible(const XL6_IrModule* m,const XL6_IrIntrinsicCall* call,
    const XL6_IrIntrinsicOperand* args)
{
    XL6_IrTypeId types[2]={0}; XL6_IrTreeQueryPlan p={0};
    if(!m || !call || !args || call->operandCount>2) return 0;
    for(size_t i=0;i<call->operandCount;++i) {
        const XL6_IrType* t=xl6IrTypeGet(&m->types,args[i].type); types[i]=args[i].type;
        if(!t || args[i].abi!=(t->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED ?
            XL6_IR_INTRINSIC_OPERAND_BORROW : XL6_IR_INTRINSIC_OPERAND_VALUE)) return 0;
    }
    if(!xl6IrTreeQuerySignatureCompatible(m,call->runtimeResourceOp,call->receiverType,call->resultType,types,call->operandCount) ||
        !xl6IrTreeQueryPlanBuild(m,call->runtimeResourceOp,call->receiverType,call->resultType,&p)) return 0;
    return call->payloadType==p.valueType && call->runtimeResourceKeyType==p.keyType &&
        call->runtimeResourcePayloadAbi==xl6IrTypeGet(&m->types,p.valueSchemaType)->sequenceLayout.transferAbi &&
        call->runtimeResourceKeyPayloadAbi==xl6IrTypeGet(&m->types,p.keySchemaType)->sequenceLayout.transferAbi &&
        call->runtimeResourceKeyOrderAbi==xl6IrAvlKeyOrderAbi(m,p.ownerType) &&
        call->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE &&
        call->resultAbi==xl6IrRuntimeResourceResultAbiForType(call->runtimeResourceOp,xl6IrTypeGet(&m->types,p.resultType));
}
int xl6IrTreeQueryInstructionCompatible(const XL6_IrModule* m,const XL6_IrInstruction* ins)
{
    if(!m || !ins || ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !ins->result ||
        ins->operandCount>2 || ins->firstOperand>m->operandCount || ins->operandCount>m->operandCount-ins->firstOperand ||
        ins->immediateText || ins->immediateData || ins->immediateSize || ins->opcode || ins->targetSymbol ||
        ins->targetValue || ins->calleeType || ins->runtimeIdentityType || ins->sequenceAdmissionIdentitySet) return 0;
    const XL6_IrTreeQuerySpec* spec=xl6IrTreeQuerySpecRuntime(ins->runtimeOp);
    XL6_IrTypeId args[2]={0}; XL6_IrTreeQueryPlan p={0};
    for(size_t i=0;i<ins->operandCount;++i) {
        const XL6_IrInstruction* def=xl6IrValueDefinition(m,m->operands[ins->firstOperand+i],NULL);
        if(!def) return 0;
        args[i]=def->type;
    }
    if(!spec || !xl6IrTreeQuerySignatureCompatible(m,spec->source,args[0],ins->type,args,ins->operandCount) ||
        !xl6IrTreeQueryPlanBuild(m,spec->source,args[0],ins->type,&p)) return 0;
    return ins->payloadType==p.valueType && ins->runtimeResourceKeyType==p.keyType &&
        ins->runtimeResourcePayloadAbi==xl6IrTypeGet(&m->types,p.valueSchemaType)->sequenceLayout.transferAbi &&
        ins->runtimeResourceKeyPayloadAbi==xl6IrTypeGet(&m->types,p.keySchemaType)->sequenceLayout.transferAbi &&
        ins->runtimeResourceKeyOrderAbi==xl6IrAvlKeyOrderAbi(m,p.ownerType);
}
