/* A pointer/unsigned-length pair becomes one owning language bytes value.
 * All physical and ownership facts close here, before C emission. */
const XL6_ForeignOwnedBytesContract* xl6IrForeignOwnedBytesGet(const XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    if(!m || !t || m->foreignOwnedBytesCount>m->foreignOwnedBytesCapacity ||
       m->foreignOwnedBytesCount>m->nodeCount || (m->foreignOwnedBytesCount && !m->foreignOwnedBytes))return NULL;
    const XL6_ForeignOwnedBytesContract* found=NULL;
    for(size_t i=0;i<m->foreignOwnedBytesCount;++i)if(m->foreignOwnedBytes[i].declaration==t->declaration){
        if(found)return NULL;
        found=&m->foreignOwnedBytes[i];
    }
    return found;
}
static int xl6IrOwnedBytesValid(const XL6_IrModule* m,const XL6_ForeignOwnedBytesContract* row)
{
    const XL6_IrNode* n=xl6IrNodeGet(m,row->declaration);
    if(!n || n->kind!=XL6_IR_NODE_FUNC_DECL ||
       (n->sourceKind!=XL6_AST_EXTERN_FUNC && n->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC && n->sourceKind!=XL6_AST_GET) ||
       n->childCount<3 || n->firstChild>m->childCount || n->childCount>m->childCount-n->firstChild || !m->children ||
       n->foreignCall.valueOutput!=XL6_FOREIGN_VALUE_OUTPUT_OWNED_BYTES_STATUS ||
       row->dataIndex>=row->sizeIndex || row->sizeIndex>=64 ||
       !xl6ForeignOwnedBytesPointerCName(row->pointerAbi) || !xl6ForeignOwnedBytesSizeCName(row->sizeAbi) ||
       n->foreignCall.outputIndexPlusOne!=row->dataIndex+1 || n->foreignCall.successCode!=row->successCode ||
       n->foreignCall.rollbackSymbol || n->foreignCall.inplaceOutput || n->foreignCall.borrowedViewResult || n->foreignCall.receiverDereference ||
       !(n->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (n->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)))return 0;
    const XL6_IrNode* returns=xl6IrNodeGet(m,m->children[n->firstChild+2]);
    if(!returns || returns->kind!=XL6_IR_NODE_LIST || returns->childCount!=1 || returns->firstChild>=m->childCount)return 0;
    const XL6_IrNode* result=xl6IrNodeGet(m,m->children[returns->firstChild]);
    const XL6_IrType* type=result ? xl6IrTypeGet(&m->types,result->type):NULL;
    if(!type || type->kind!=XL6_IR_TYPE_BUILTIN || type->paramCount || type->cAbi!=XL6_IR_C_ABI_MANAGED_BYTES || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED)return 0;
    XL6_IrCallTarget target={0};target.declaration=row->declaration;target.foreignHeaderIndexPlusOne=xl6IrForeignFunctionHeaderIndex(m,row->declaration);
    const XL6_IrType* owner=xl6IrTypeGet(&m->types,n->declarationOwnerType);
    return xl6IrForeignFunctionHeaderGet(m,&target) || (owner && owner->foreignHeader);
}
static int xl6IrVerifyOwnedBytes(const XL6_IrModule* m,XL6_DiagList* diags)
{
    if(m->foreignOwnedBytesCount>m->foreignOwnedBytesCapacity || m->foreignOwnedBytesCount>m->nodeCount ||
       (m->foreignOwnedBytesCount && !m->foreignOwnedBytes))goto invalid;
    for(size_t i=0;i<m->foreignOwnedBytesCount;++i)if(!xl6IrOwnedBytesValid(m,&m->foreignOwnedBytes[i]))goto invalid;
    for(size_t n=0;n<m->nodeCount;++n){
        size_t count=0;for(size_t i=0;i<m->foreignOwnedBytesCount;++i)count+=m->foreignOwnedBytes[i].declaration==n+1;
        if(count!=(m->nodes[n].foreignCall.valueOutput==XL6_FOREIGN_VALUE_OUTPUT_OWNED_BYTES_STATUS ? 1u:0u))goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"owned bytes output needs one sealed resident-XRT allocation contract, exact bytes result, header and pointer/unsigned-length slots");
}
