/* A pointer/unsigned-length pair becomes one owning language bytes value.
 * All physical and ownership facts close here, before C emission. */
const XL6_ForeignOwnedOutputContract* xl6IrForeignOwnedOutputGet(const XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    if(!m || !t || m->foreignOwnedOutputsCount>m->foreignOwnedOutputsCapacity ||
       m->foreignOwnedOutputsCount>m->nodeCount || (m->foreignOwnedOutputsCount && !m->foreignOwnedOutputs))return NULL;
    const XL6_ForeignOwnedOutputContract* found=NULL;
    for(size_t i=0;i<m->foreignOwnedOutputsCount;++i)if(m->foreignOwnedOutputs[i].declaration==t->declaration){
        if(found)return NULL;
        found=&m->foreignOwnedOutputs[i];
    }
    return found;
}
static int xl6IrOwnedOutputValid(const XL6_IrModule* m,const XL6_ForeignOwnedOutputContract* row)
{
    const XL6_IrNode* n=xl6IrNodeGet(m,row->declaration);
    if(!n || n->kind!=XL6_IR_NODE_FUNC_DECL ||
       (n->sourceKind!=XL6_AST_EXTERN_FUNC && n->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC && n->sourceKind!=XL6_AST_GET) ||
       n->childCount<3 || n->firstChild>m->childCount || n->childCount>m->childCount-n->firstChild || !m->children ||
       (row->kind!=XL6_FOREIGN_OWNED_OUTPUT_BYTES && row->kind!=XL6_FOREIGN_OWNED_OUTPUT_TEXT) ||
       n->foreignCall.valueOutput!=(row->kind==XL6_FOREIGN_OWNED_OUTPUT_TEXT ? XL6_FOREIGN_VALUE_OUTPUT_OWNED_TEXT_STATUS:XL6_FOREIGN_VALUE_OUTPUT_OWNED_BYTES_STATUS) ||
       (row->kind==XL6_FOREIGN_OWNED_OUTPUT_TEXT && row->pointerAbi!=XL6_FOREIGN_OWNED_OUTPUT_CHAR) ||
       row->dataIndex>=row->sizeIndex || row->sizeIndex>=64 ||
       !xl6ForeignOwnedOutputPointerCName(row->pointerAbi) || !xl6ForeignOwnedOutputSizeCName(row->sizeAbi) ||
       n->foreignCall.outputIndexPlusOne!=row->dataIndex+1 || n->foreignCall.successCode!=row->successCode ||
       n->foreignCall.rollbackSymbol || n->foreignCall.inplaceOutput || n->foreignCall.borrowedViewResult || n->foreignCall.receiverDereference ||
       !(n->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (n->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)))return 0;
    const XL6_IrNode* returns=xl6IrNodeGet(m,m->children[n->firstChild+2]);
    if(!returns || returns->kind!=XL6_IR_NODE_LIST || returns->childCount!=1 || returns->firstChild>=m->childCount)return 0;
    const XL6_IrNode* result=xl6IrNodeGet(m,m->children[returns->firstChild]);
    const XL6_IrType* type=result ? xl6IrTypeGet(&m->types,result->type):NULL;
    if(!type || type->kind!=XL6_IR_TYPE_BUILTIN || type->paramCount || type->cAbi!=(row->kind==XL6_FOREIGN_OWNED_OUTPUT_TEXT ? XL6_IR_C_ABI_MANAGED_STRING:XL6_IR_C_ABI_MANAGED_BYTES) || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED)return 0;
    XL6_IrCallTarget target={0};target.declaration=row->declaration;target.foreignHeaderIndexPlusOne=xl6IrForeignFunctionHeaderIndex(m,row->declaration);
    const XL6_IrType* owner=xl6IrTypeGet(&m->types,n->declarationOwnerType);
    return xl6IrForeignFunctionHeaderGet(m,&target) || (owner && owner->foreignHeader);
}
static int xl6IrVerifyOwnedOutputs(const XL6_IrModule* m,XL6_DiagList* diags)
{
    if(m->foreignOwnedOutputsCount>m->foreignOwnedOutputsCapacity || m->foreignOwnedOutputsCount>m->nodeCount ||
       (m->foreignOwnedOutputsCount && !m->foreignOwnedOutputs))goto invalid;
    for(size_t i=0;i<m->foreignOwnedOutputsCount;++i)if(!xl6IrOwnedOutputValid(m,&m->foreignOwnedOutputs[i]))goto invalid;
    for(size_t n=0;n<m->nodeCount;++n){
        size_t count=0;for(size_t i=0;i<m->foreignOwnedOutputsCount;++i)count+=m->foreignOwnedOutputs[i].declaration==n+1;
        if(count!=(xl6ForeignValueOutputIsOwned(m->nodes[n].foreignCall.valueOutput) ? 1u:0u))goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"owned bytes output needs one sealed resident-XRT allocation contract, exact bytes result, header and pointer/unsigned-length slots");
}
