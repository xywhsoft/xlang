/* Native borrowed records are stack-only physical storage. No raw pointer
 * field may masquerade as a trivial language value. */
static int xl6SemForeignOutputRecordPod(XL6_SemContext* ctx,const XL6_AstNode* type,size_t depth)
{
    type=xl6SemResolveAliasType(ctx,type);
    if(!type || depth>16 || xl6SemTypeIsNullable(type))return 0;
    if(xl6SemForeignPackScalarCName(type))return 1;
    const XL6_AstNode* record=type->text && !type->childCount ? xl6SemFindTypeDecl(ctx,type->text):NULL;
    if(!record || record->kind!=XL6_AST_C_RECORD || !record->childCount)return 0;
    const XL6_AstNode* fields=record->children[0];
    for(size_t f=0;fields && f<fields->childCount;++f){
        const XL6_AstNode* field=fields->children[f];
        if(!field || !field->childCount || !xl6SemForeignOutputRecordPod(ctx,field->children[0],depth+1))return 0;
    }
    return fields!=NULL;
}
static int xl6SemForeignOutputRecord(XL6_SemContext* ctx,XL6_AstNode* node,
    const XL6_AstNode* attrs,const XL6_AstNode* ownerClass,const XL6_ForeignCallFacts* facts,size_t inputs,size_t receiver)
{
    XL6_ForeignOutputRecordContract record;char header[256]={0};XL6_ForeignClassDecl owner={0};
    const XL6_AstNode* returns=node->childCount>2 ? node->children[2]:NULL;size_t count=0;
    if(ownerClass && xl6ForeignClassRead(ownerClass,&owner,NULL)==1)strcpy(header,owner.header);
    for(size_t a=0;attrs && a<attrs->childCount;++a)if(xl6ForeignFunctionHeaderRead(attrs->children[a],header,sizeof(header)))break;
    if(!xl6ForeignOutputRecordForDeclaration(node,&record) || !header[0] || !returns || !returns->childCount || returns->childCount>64 ||
        record.physicalIndex+1!=facts->outputIndexPlusOne || !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
        (node->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) || facts->borrowedViewResult || facts->rollbackSymbol ||
        xl6ForeignOutputLogicalForDeclaration(node,facts,inputs,receiver)==SIZE_MAX)goto invalid;
    for(size_t a=0;attrs && a<attrs->childCount;++a){
        const XL6_AstNode* attr=attrs->children[a];XL6_ForeignOutputFieldContract field;XL6_ForeignOutputViewContract view;
        if(!attr || !attr->text || strcmp(attr->text,"c_output_field"))continue;
        if(!xl6ForeignOutputFieldRead(attr,&field,&view) || field.ordinal>=returns->childCount)goto invalid;
        ++count;const XL6_AstNode* result=xl6SemResolveAliasType(ctx,returns->children[field.ordinal]);
        if(view.conversion){
            const XL6_AstNode* inner=xl6SemTypeIsNullable(result) ? xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(result)):result;
            XL6_ForeignClassDecl contract={0};const XL6_AstNode* declaration=xl6SemForeignViewDeclaration(ctx,inner,&contract);
            const XL6_AstNode* input=xl6SemClassValueType(ctx,inner);
            const XL6_AstNode* hold=xl6SemForeignOwnershipHelper(ctx,attr->children[3],input,1);
            const XL6_AstNode* drop=xl6SemForeignOwnershipHelper(ctx,attr->children[5],input,0);
            if(!declaration || contract.lifetime!=XL6_FOREIGN_LIFETIME_OWNED || !contract.header[0] || contract.watch[0] ||
                !hold || !drop || (node->flags&XL6_AST_FLAG_FOREIGN_PURE))goto invalid;
            attr->children[3]->semanticSymbolId=hold->semanticSymbolId;attr->children[5]->semanticSymbolId=drop->semanticSymbolId;
        }else if(!xl6SemForeignOutputRecordPod(ctx,result,0))goto invalid;
    }
    if(count!=returns->childCount)goto invalid;
    return 1;
invalid:xl6SemErrorNode(ctx,node,"borrowed C record output requires exact POD fields or retained independent resources, one field per logical result, an authoritative header and a complete physical output slot");return 0;
}
