/* Exact logical Tree signatures, shared by open templates and closed native
 * admission. A dependent source contract never guesses a native key codec. */
static int xl6IrTreeLogicalSignature(const XL6_IrModule* m,XL6_IrRuntimeResourceOp op,
    XL6_IrTypeId receiver,XL6_IrTypeId resultId,const XL6_IrTypeId* args,size_t count)
{
    if(!m || !xl6IrRuntimeResourceOpIsAvlTree(op))return 0;
    bool creates=op==XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CREATE;
    XL6_IrTypeId ownerId=creates ? resultId : receiver;
    const XL6_IrType* owner=xl6IrTypeGet(&m->types,ownerId);
    const XL6_IrType* result=xl6IrTypeGet(&m->types,resultId);
    if(!owner || owner->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || owner->runtimeResourceAbi!=XL6_IR_RUNTIME_RESOURCE_AVL_TREE ||
        owner->paramCount!=2 || !owner->params)return 0;
    if(creates)return receiver==XL6_IR_TYPE_INVALID && !count;
    if(!count || !args || args[0]!=receiver)return 0;
    XL6_IrTypeId key=owner->params[0],value=owner->params[1];
    const XL6_IrType* second=count>1 ? xl6IrTypeGet(&m->types,args[1]) : NULL;
    const XL6_IrType* third=count>2 ? xl6IrTypeGet(&m->types,args[2]) : NULL;
    switch(op) {
    case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_SET:
        return count==3 && args[1]==key && args[2]==value && result && result->cAbi==XL6_IR_C_ABI_BOOL;
    case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CONTAINS:case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_REMOVE:
        return count==2 && args[1]==key && result && result->cAbi==XL6_IR_C_ABI_BOOL;
    case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_TRIM_TO:
        return count==2 && second && second->cAbi==XL6_IR_C_ABI_I64 && result && result->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64;
    case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_EQUALS:
        return count==2 && args[1]==receiver && result && result->cAbi==XL6_IR_C_ABI_BOOL;
    case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_MERGE:
        return (count==2 || count==3) && args[1]==receiver && (count==2 || (third && third->cAbi==XL6_IR_C_ABI_BOOL)) && result && result->cAbi==XL6_IR_C_ABI_BOOL;
    case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CLEAR:return count==1 && resultId==XL6_IR_TYPE_INVALID;
    case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_COUNT:case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_TRIM:
        return count==1 && result && result->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64;
    case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_KEYS:case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_VALUES:
        return count==1 && result && result->kind==XL6_IR_TYPE_GENERIC_INSTANCE && result->collectionAbi==XL6_IR_COLLECTION_ARRAY &&
            result->paramCount==1 && result->params && result->params[0]==(op==XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_KEYS ? key : value);
    case XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CLONE:return count==1 && resultId==receiver;
    default:return 0;
    }
}
static int xl6IrGenericTreeSignatureCompatible(const XL6_IrModule* m,XL6_IrNodeId origin,
    XL6_IrRuntimeResourceOp op,XL6_IrTypeId receiver,XL6_IrTypeId result,const XL6_IrTypeId* args,size_t count)
{
    if(xl6IrTreeQuerySpecResource(op))
        return xl6IrGenericTreeQuerySignatureCompatible(m,origin,op,receiver,result,args,count);
    XL6_IrTypeId owner=op==XL6_IR_RUNTIME_RESOURCE_OP_AVL_TREE_CREATE ? result : receiver;
    if(!xl6IrTreeLogicalSignature(m,op,receiver,result,args,count) || !xl6IrBuildTypeContainsTypeParam(m,owner,0))return 0;
    const XL6_IrNode* declaration=xl6IrNodeGet(m,origin);size_t depth=0;
    while(declaration && declaration->kind!=XL6_IR_NODE_FUNC_DECL && ++depth<=m->nodeCount)declaration=xl6IrNodeGet(m,declaration->parent);
    return declaration && (xl6IrBuildTypeContainsTypeParam(m,declaration->declarationOwnerType,0) || xl6IrBuildTypeContainsTypeParam(m,declaration->storageType,0));
}
