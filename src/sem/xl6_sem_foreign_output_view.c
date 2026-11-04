/* Logical owned results are distinct from their physical borrowed view slots. */
static int xl6SemForeignOutputViewAt(const XL6_AstNode* attrs,uint32 physical,
    XL6_ForeignOutputViewContract* output)
{
    for(size_t a=0;attrs && a<attrs->childCount;++a){
        XL6_ForeignOutputViewContract row;
        if(xl6ForeignOutputViewRead(attrs->children[a],&row) && row.physicalIndex==physical){
            if(output)*output=row;
            return 1;
        }
    }
    return 0;
}
static int xl6SemForeignOutputViews(XL6_SemContext* ctx,XL6_AstNode* node,
    const XL6_AstNode* attrs,const uint32* slots,size_t count)
{
    const XL6_AstNode* returns=node->childCount>2 ? node->children[2]:NULL;
    for(size_t a=0;attrs && a<attrs->childCount;++a){
        const XL6_AstNode* attr=attrs->children[a];XL6_ForeignOutputViewContract row;
        if(!attr || !attr->text || (strcmp(attr->text,"c_output_view") && strcmp(attr->text,"c_output_retain")))continue;
        if(!xl6ForeignOutputViewRead(attr,&row) || !returns || returns->childCount!=count)goto invalid;
        size_t ordinal=0;while(ordinal<count && slots[ordinal]!=row.physicalIndex)++ordinal;
        if(ordinal==count)goto invalid;
        const XL6_AstNode* result=xl6SemResolveAliasType(ctx,returns->children[ordinal]);
        const XL6_AstNode* inner=result ? xl6SemResolveAliasType(ctx,
            xl6SemTypeIsNullable(result) ? xl6SemNullableInnerType(result):result):NULL;
        if(row.conversion==XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE){
            XL6_ForeignClassDecl contract={0};
            const XL6_AstNode* view=xl6SemForeignViewDeclaration(ctx,inner,&contract);
            const XL6_AstNode* input=xl6SemClassValueType(ctx,inner);
            const XL6_AstNode* retain=xl6SemForeignOwnershipHelper(ctx,attr->children[1],input,1);
            const XL6_AstNode* rollback=xl6SemForeignOwnershipHelper(ctx,attr->children[3],input,0);
            if(!view || contract.lifetime!=XL6_FOREIGN_LIFETIME_OWNED || !contract.header[0] ||
                contract.watch[0] || contract.scope[0] || !retain || !rollback ||
                !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
                (node->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_OWNED_STRING)))goto invalid;
            attr->children[1]->semanticSymbolId=retain->semanticSymbolId;
            attr->children[3]->semanticSymbolId=rollback->semanticSymbolId;
            continue;
        }
        const char* name=row.conversion==XL6_FOREIGN_OUTPUT_CONVERSION_TEXT ? "string":"bytes";
        if(!inner || inner->childCount || !xl6TextEquals(inner->text,name))goto invalid;
    }
    return 1;
invalid:xl6SemErrorNode(ctx,node,"native output conversion requires an exact string/bytes result or independent owned root with typed retain/release helpers at one complete physical output slot");return 0;
}
