/* Physical borrowed-record outputs. These descriptors assert that no field
 * carries a producer-owned responsibility requiring a record destructor. */
static inline int xl6ForeignOutputRecordRead(const XL6_AstNode* attr,XL6_ForeignOutputRecordContract* output)
{
    XL6_ForeignOutputRecordContract row={0};int32 physical;char ownership[16];
    if(!attr || !attr->text || strcmp(attr->text,"c_output_record") || attr->childCount!=3 ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !xl6ForeignQuotedText(attr->children[1],row.recordCName,sizeof(row.recordCName)) || !xl6ForeignIdentifier(row.recordCName) ||
        !xl6ForeignQuotedText(attr->children[2],ownership,sizeof(ownership)) || strcmp(ownership,"borrowed"))return 0;
    row.physicalIndex=(uint32)physical;if(output)*output=row;return 1;
}
static inline int xl6ForeignOutputFieldRead(const XL6_AstNode* attr,XL6_ForeignOutputFieldContract* output,
    XL6_ForeignOutputViewContract* conversion)
{
    XL6_ForeignOutputFieldContract row={0};XL6_ForeignOutputViewContract view={0};int32 ordinal;
    if(!attr || !attr->text || strcmp(attr->text,"c_output_field") || (attr->childCount!=2 && attr->childCount!=6) ||
        !xl6ForeignDecimalInt(attr->children[0],&ordinal) || ordinal<0 || ordinal>=64 ||
        !xl6ForeignQuotedText(attr->children[1],row.field,sizeof(row.field)) || !xl6ForeignIdentifier(row.field) || row.field[0]=='_')return 0;
    row.ordinal=(uint32)ordinal;
    if(attr->childCount==6){
        char kind[16],success[16];
        if(!xl6ForeignQuotedText(attr->children[2],kind,sizeof(kind)) || strcmp(kind,"retain") ||
            !attr->children[3] || attr->children[3]->kind!=XL6_AST_NAME || !attr->children[3]->text ||
            !xl6ForeignQuotedText(attr->children[4],success,sizeof(success)) ||
            !attr->children[5] || attr->children[5]->kind!=XL6_AST_NAME || !attr->children[5]->text)return 0;
        view.retainSuccess=!strcmp(success,"zero") ? XL6_FOREIGN_RETAIN_ZERO:
            !strcmp(success,"positive") ? XL6_FOREIGN_RETAIN_POSITIVE:0;
        if(!view.retainSuccess)return 0;
        view.ordinal=row.ordinal;view.conversion=XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE;
        view.retainSymbol=attr->children[3]->semanticSymbolId;view.rollbackSymbol=attr->children[5]->semanticSymbolId;
    }
    if(output)*output=row;
    if(conversion)*conversion=view;
    return 1;
}
static inline int xl6ForeignOutputRecordForDeclaration(const XL6_AstNode* node,XL6_ForeignOutputRecordContract* output)
{
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);size_t count=0;XL6_ForeignOutputRecordContract row;
    for(size_t i=0;attrs && i<attrs->childCount;++i){XL6_ForeignOutputRecordContract found;
        if(xl6ForeignOutputRecordRead(attrs->children[i],&found)){row=found;++count;}}
    if(count!=1)return 0;
    if(output)*output=row;
    return 1;
}
