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
        if(!attr || !attr->text || strcmp(attr->text,"c_output_view"))continue;
        if(!xl6ForeignOutputViewRead(attr,&row) || !returns || returns->childCount!=count)goto invalid;
        size_t ordinal=0;while(ordinal<count && slots[ordinal]!=row.physicalIndex)++ordinal;
        if(ordinal==count)goto invalid;
        const XL6_AstNode* result=xl6SemResolveAliasType(ctx,returns->children[ordinal]);
        const XL6_AstNode* inner=result ? xl6SemResolveAliasType(ctx,
            xl6SemTypeIsNullable(result) ? xl6SemNullableInnerType(result):result):NULL;
        const char* name=row.conversion==XL6_FOREIGN_OUTPUT_CONVERSION_TEXT ? "string":"bytes";
        if(!inner || inner->childCount || !xl6TextEquals(inner->text,name))goto invalid;
    }
    return 1;
invalid:xl6SemErrorNode(ctx,node,"@c_output_view requires an exact string/bytes result (optionally nullable) at one complete physical output slot");return 0;
}
