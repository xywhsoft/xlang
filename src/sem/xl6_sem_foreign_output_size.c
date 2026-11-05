/* Declaration-time validation only; execution consumes the sealed IR row. */
static int xl6SemForeignOutputSizes(XL6_SemContext* ctx,XL6_AstNode* node,
    const XL6_AstNode* attrs,const XL6_ForeignCallFacts* facts)
{
    const XL6_AstNode* returns=node->childCount>2 ? node->children[2]:NULL;
    uint32 slots[64]={facts->outputIndexPlusOne ? facts->outputIndexPlusOne-1:0};size_t count=1;
    if(xl6ForeignValueOutputBase(facts->valueOutput)==XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS || xl6ForeignValueOutputBase(facts->valueOutput)==XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID)
        for(size_t a=0;attrs && a<attrs->childCount;++a)
            if(xl6ForeignMultiOutputRead(attrs->children[a],slots,&count))break;
    for(size_t a=0;attrs && a<attrs->childCount;++a){
        const XL6_AstNode* attr=attrs->children[a];XL6_ForeignOutputSizeContract row;
        if(!attr || !attr->text || strcmp(attr->text,"c_output_size"))continue;
        if(!xl6ForeignOutputSizeRead(attr,&row) || !facts->valueOutput || !returns)goto invalid;
        if(xl6ForeignValueOutputIsRecord(facts->valueOutput)){
            XL6_ForeignOutputRecordContract group;
            if(!xl6ForeignOutputRecordForDeclaration(node,&group) || row.physicalIndex!=group.physicalIndex)goto invalid;
            /* This physical struct need not be a pointer-free logical record.
             * Its field qualification/range is checked against the C header. */
            continue;
        }
        if(returns->childCount!=count)goto invalid;
        size_t ordinal=0;while(ordinal<count && slots[ordinal]!=row.physicalIndex)++ordinal;
        if(ordinal==count)goto invalid;
        const XL6_AstNode* result=xl6SemResolveAliasType(ctx,returns->children[ordinal]);
        const XL6_AstNode* record=result && result->text && !result->childCount ? xl6SemFindTypeDecl(ctx,result->text):NULL;
        if(xl6ForeignRecordRead(record,NULL,NULL)!=1 || !record->childCount)goto invalid;
        const XL6_AstNode* fields=record->children[0];size_t found=0;
        for(size_t f=0;fields && f<fields->childCount;++f){
            const XL6_AstNode* field=fields->children[f];char cName[64];
            if(!xl6ForeignRecordFieldRead(field,cName,sizeof(cName),NULL) || strcmp(cName,row.field))continue;
            const XL6_AstNode* type=field->childCount ? xl6SemResolveAliasType(ctx,field->children[0]):NULL;
            if(!type || !type->text || type->childCount ||
                (strcmp(type->text,"c_uchar") && strcmp(type->text,"c_ushort") && strcmp(type->text,"c_uint") &&
                 strcmp(type->text,"c_ulong") && strcmp(type->text,"c_ulonglong") && strcmp(type->text,"c_size") && strcmp(type->text,"c_uintptr")))goto invalid;
            ++found;
        }
        if(found!=1)goto invalid;
    }
    return 1;
invalid:xl6SemErrorNode(ctx,node,"@c_output_size requires one exact output C record and an unsigned C integer member");return 0;
}
