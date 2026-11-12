static int xl6SemForeignOwnedOutput(XL6_SemContext* ctx,XL6_AstNode* node,
    const XL6_AstNode* attrs,const XL6_AstNode* ownerClass,const XL6_ForeignCallFacts* facts,size_t inputs,size_t receiver)
{
    XL6_ForeignOwnedOutputContract row={0};size_t found=0;char header[256]={0};XL6_ForeignClassDecl owner={0};
    for(size_t i=0;attrs && i<attrs->childCount;++i){
        found+=xl6ForeignOwnedOutputRead(attrs->children[i],&row);
        if(xl6ForeignFunctionHeaderRead(attrs->children[i],header,sizeof(header)))continue;
    }
    if(!header[0] && ownerClass && xl6ForeignClassRead(ownerClass,&owner,NULL)==1)strcpy(header,owner.header);
    const XL6_AstNode* returns=node->childCount>2 ? node->children[2]:NULL;
    const XL6_AstNode* type=returns && returns->childCount==1 ? xl6SemResolveAliasType(ctx,returns->children[0]):NULL;
    if(found==1 && type && !type->childCount && xl6TextEquals(type->text,row.kind==XL6_FOREIGN_OWNED_OUTPUT_TEXT ? "string":"bytes") && header[0] &&
       (node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) &&
       xl6ForeignOutputLogicalForDeclaration(node,facts,inputs,receiver)!=SIZE_MAX)return 1;
    xl6SemErrorNode(ctx,node,"owned output requires one matching nonnullable bytes or UTF-8 string result, authoritative header, no-raise native status producer and complete pointer/length output slots from the exact resident XRT allocator");return 0;
}
