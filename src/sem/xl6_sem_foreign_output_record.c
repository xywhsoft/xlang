/* Native borrowed records are stack-only physical storage. No raw pointer
 * field may masquerade as a trivial language value. */
static int xl6SemForeignOutputWatchedChild(XL6_SemContext* ctx,const XL6_AstNode* ownerClass,
    const XL6_AstNode* result)
{
    XL6_ForeignClassDecl owner={0},child={0},root={0};
    if(xl6ForeignClassRead(ownerClass,&owner,NULL)!=1 ||
        (owner.lifetime!=XL6_FOREIGN_LIFETIME_OWNED && owner.lifetime!=XL6_FOREIGN_LIFETIME_OWNER))return 0;
    const XL6_AstNode* resultDecl=xl6SemForeignViewDeclaration(ctx,result,&child);
    if(!resultDecl || child.lifetime!=XL6_FOREIGN_LIFETIME_OWNER || !child.policy[0] || !child.watch[0])return 0;
    const XL6_AstNode* ownerRoot=xl6SemForeignRootDeclaration(ctx,ownerClass,&owner);
    return ownerRoot && ownerRoot==xl6SemForeignRootDeclaration(ctx,resultDecl,&child) &&
        xl6ForeignClassRead(ownerRoot,&root,NULL)==1 && root.scope[0];
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
        if(view.conversion==XL6_FOREIGN_OUTPUT_CONVERSION_CSTRING){
            const XL6_AstNode* inner=xl6SemTypeIsNullable(result) ? xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(result)):result;
            if(!inner || inner->childCount || !xl6TextEquals(inner->text,"string"))goto invalid;
        }else if(view.conversion==XL6_FOREIGN_OUTPUT_CONVERSION_BORROWED_VALUE){
            if(!result || result->childCount || !xl6TextEquals(result->text,"var") || (node->flags&XL6_AST_FLAG_FOREIGN_PURE))goto invalid;
        }else if(view.conversion==XL6_FOREIGN_OUTPUT_CONVERSION_OWNER_VIEW){
            if(!receiver || (facts->controlViews&UINT64_C(1)) ||
                (node->flags&XL6_AST_FLAG_FOREIGN_PURE) ||
                !xl6SemForeignOutputWatchedChild(ctx,ownerClass,result))goto invalid;
        }else if(view.conversion){
            const XL6_AstNode* inner=xl6SemTypeIsNullable(result) ? xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(result)):result;
            XL6_ForeignClassDecl contract={0};const XL6_AstNode* declaration=xl6SemForeignViewDeclaration(ctx,inner,&contract);
            const XL6_AstNode* input=xl6SemClassValueType(ctx,inner);
            const XL6_AstNode* hold=xl6SemForeignOwnershipHelper(ctx,attr->children[3],input,1);
            const XL6_AstNode* drop=xl6SemForeignOwnershipHelper(ctx,attr->children[5],input,0);
            if(!declaration || contract.lifetime!=XL6_FOREIGN_LIFETIME_OWNED || !contract.header[0] || contract.watch[0] ||
                !hold || !drop || (node->flags&XL6_AST_FLAG_FOREIGN_PURE))goto invalid;
            attr->children[3]->semanticSymbolId=hold->semanticSymbolId;attr->children[5]->semanticSymbolId=drop->semanticSymbolId;
        }else if(!xl6SemForeignTrivialCValue(ctx,result,0))goto invalid;
    }
    if(count!=returns->childCount)goto invalid;
    return 1;
invalid:xl6SemErrorNode(ctx,node,"borrowed C record output requires an authoritative header and an exact POD, C-string, retained resource, borrowed xvalue or same-root watched child per result");return 0;
}
